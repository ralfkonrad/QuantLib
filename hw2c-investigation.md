# Investigation: HW2C Bermudan Swaption Pricing Discrepancy

## Summary

The `HW2CTreeSwaptionEngine` produces systematically higher Bermudan swaption prices compared to the standard `TreeSwaptionEngine` with `HullWhite`, even when both engines use the **same single flat yield curve** for discounting and forward projection. The root cause is a **day-counter mismatch** in how `HW2CDiscretizedSwap::addFloatingCoupon` constructs the forward projection interval on the lattice. This bug inflates every floating coupon, which in turn contaminates the model calibration, compounding the pricing error.

## Observed Pricing Results

```
Payer Bermudan swaption, 1000 notional, flat curve at 4.875825%

                     ATM (5.0%)    OTM (6.0%)    ITM (4.0%)
HW (tree, analytic):   12.928        2.5139        42.252
HW (tree, numerical):  13.145        2.6157        42.346
G2 (tree):             14.132        3.2231        42.604
BK (tree):             13.017        3.2732        41.812
HW2C (tree):           15.494        3.5913        45.351
```

The HW2C engine overprices by 2.3–2.6 (ATM), 1.0–1.1 (OTM), and 3.0–3.1 (ITM) relative to the comparable single-factor HW tree engine.

Calibrated parameters also differ:

```
HW (analytic):    a = 0.046416,  σ = 0.0058693
HW (numerical):   a = 0.055966,  σ = 0.0060993
HW2C (numerical): a = 0.049693,  σ = 0.0063831
```

## Root Cause: Day-Counter Mismatch in Forward Projection Interval

### Location

`ql/experimental/hullwhitewithtwocurves/pricingengines/swap/hw2cdiscretizedswap.cpp`, constructor and `addFloatingCoupon`.

### The Bug

In the constructor of `HW2CDiscretizedSwap` (lines 109–110):

```cpp
indexStartTimes_[i] = resetTime;
indexEndTimes_[i] = resetTime + spanning;
```

where:
- `resetTime = dayCounter.yearFraction(referenceDate, args.floatingResetDates[i])`
  uses the **lattice day counter** (`Actual365Fixed` from the term structure)
- `spanning = args.floatingAccrualTimes[i]` = `coupon->accrualPeriod()`
  uses the **floating leg day counter** (`Actual360` for Euribor)

This **mixes two different day-count conventions** in a single time-axis computation. The resulting `indexEndTimes_[i]` does not correspond to any actual calendar date on the lattice time axis.

### Quantitative Impact

For a 6-month Euribor coupon over a typical ~182-day period:

| Convention    | Year Fraction |
|:--------------|:--------------|
| Actual365Fixed | 182/365 ≈ 0.49863 |
| Actual360      | 182/360 ≈ 0.50556 |

The Actual360 fraction is ~1.4% larger. This means `indexEndTimes_[i]` is pushed ~0.007 years further out than the true accrual end date. The forward bond `P(t_reset, t_end)` is therefore computed for a slightly longer interval, producing a **slightly lower** discount factor and hence a **slightly higher** implied forward rate at every tree node.

### How It Inflates Pricing

In `addFloatingCoupon` (line 246):

```cpp
const Real projectionDiscount = forwardBond.values()[j];
const Real forwardRate = (1.0 / projectionDiscount - 1.0) / spanningTime;
```

The `projectionDiscount` is `P(t_reset, t_reset + τ_Act360)` instead of the correct `P(t_reset, t_accrualEnd_Act365)`. Since the forward bond interval is too long:
- `projectionDiscount` is too small
- `forwardRate` is too large
- Every floating coupon is systematically overstated

For a payer swap (receive float, pay fixed), this means the swap value at every tree node is biased upward, directly increasing the Bermudan swaption exercise value.

### Mathematical Comparison

**Standard `DiscretizedSwap::addFloatingCoupon`** uses the telescoping identity:

```
coupon_j = N × (1 − P_j(t_reset, t_pay)) + N × s × τ × P_j(t_reset, t_pay)
```

This is exact because it uses a single discount bond `P(t_reset, t_pay)` from the **same** lattice, where the payment time is correctly placed on the lattice time axis.

**`HW2CDiscretizedSwap::addFloatingCoupon`** computes:

```
forwardRate_j = (1/P_fwd(t_reset, t_reset + τ_Actual360) − 1) / τ_Actual360
couponAmount  = N × τ_Actual360 × (forwardRate_j + s)
discounted_j  = couponAmount × P_disc(t_reset, t_pay)
```

In the **single-curve limit** (identical discount and forward curves), if `indexEndTime` were correctly computed using the lattice day counter, then at each node:

```
τ × ((1/P(t,t+τ) − 1) / τ) × P(t, t_pay)
= (1/P(t, t+τ) − 1) × P(t, t_pay)
```

When `t+τ = t_pay` (i.e., the forward period end equals the payment date), this reduces to `1 − P(t, t_pay)`, matching the standard formula exactly. **But** with the day-counter mismatch, `t+τ_Act360 ≠ t_pay`, breaking this identity.

## Secondary Effect: Calibration Contamination

The same `HW2CTreeSwaptionEngine` is used during model calibration (line 252-253 in the example):

```cpp
swaptions[i]->setPricingEngine(
    ext::make_shared<HW2CTreeSwaptionEngine>(modelHW2C, 30));
```

Because the engine systematically overvalues the floating leg, it produces biased prices for the calibration swaptions. The optimizer then adjusts `a` and `σ` to compensate, yielding different calibrated parameters:

```
HW (numerical): a = 0.055966,  σ = 0.0060993
HW2C:           a = 0.049693,  σ = 0.0063831
```

The higher `σ` (+4.7%) and lower `a` (−11.2%) further amplify the Bermudan overpricing beyond the direct coupon-level error, because Bermudan swaption values are highly sensitive to volatility.

## Other Investigated Factors

### Tree Node Alignment (Not a cause)

Both the discount and forward trees are built by `HW2CModel::discountTree()` and `forwardTree()`, which each call `HullWhite::tree(timeGrid)`. The trinomial tree geometry is determined by the underlying Ornstein-Uhlenbeck process (parameters `a` and `σ`) and the `TimeGrid`, **not** by the term structure. With identical `a`, `σ`, and the same `TimeGrid`, both trees have:
- Identical node counts at every time step
- Identical `x`-coordinates (short-rate states)
- Identical branching probabilities

The term structure only affects the **fitted drift** (the `TermStructureFittingParameter` φ) and discount factors used in state-price calculations. When both curves are the same, the two trees are numerically identical. Node index `j` maps to the same state in both trees.

### Jensen's Inequality / Convexity (Not a cause in single-curve limit)

If the forward projection interval and discount interval were identical, the HW2C formula collapses to the standard telescoping identity node-by-node:

```
τ × ((1/P − 1) / τ) × P = 1 − P
```

There is no convexity correction needed because the computation happens at each tree node (not as an expectation over nodes). Any Jensen effect would only matter if computing E[f(P)] vs f(E[P]), which is not what happens here.

### Additional Mandatory Times (Minor effect)

`HW2CDiscretizedSwap::mandatoryTimes()` includes `indexStartTimes_` and `indexEndTimes_` beyond what the standard `DiscretizedSwap` uses. This produces a denser `TimeGrid`, which could marginally affect pricing through different tree discretization. However, this is a refinement effect, not a systematic bias, and cannot account for the magnitude of the observed discrepancy.

## Proposed Fix

### Immediate Fix

Replace the arithmetic `indexEndTimes_[i] = resetTime + spanning` with a proper date-to-time conversion using the lattice day counter. For a `VanillaSwap` where the floating payment date coincides with the accrual end date (no payment lag), the simplest fix is:

```cpp
// Before (buggy):
indexStartTimes_[i] = resetTime;
indexEndTimes_[i] = resetTime + spanning;

// After (correct):
indexStartTimes_[i] = resetTime;
indexEndTimes_[i] = dayCounter.yearFraction(referenceDate, args.floatingPayDates[i]);
```

This ensures the forward bond interval is measured on the same time axis as all other lattice times.

### Robust Long-Term Fix

For full dual-curve generality (where the forward index period may differ from the payment period, e.g., with payment lags or different index tenors), the `VanillaSwap::arguments` struct should be extended to include the **actual floating accrual end dates** (or the index value dates/maturity dates). The forward projection bond would then use:

```cpp
indexEndTimes_[i] = dayCounter.yearFraction(referenceDate, args.floatingAccrualEndDates[i]);
```

### Validation After Fix

1. **Single-curve parity test**: With identical discount and forward curves, `HW2CTreeSwaptionEngine` should produce prices within tree-discretization tolerance of `TreeSwaptionEngine` for both European and Bermudan swaptions.
2. **Recalibrate**: After fixing the coupon construction, recalibrate `HW2CModel` and verify the calibrated parameters converge toward the standard HW numerical parameters.
3. **Re-run BermudanSwaption example**: The HW2C prices should move to within ~0.1–0.3 of the standard HW tree prices (remaining differences due to different calibration paths and grid density).
4. **Regression test**: Add a test with `Actual365Fixed` curve and `Actual360` floating leg (the exact mismatch scenario) to prevent reintroduction of this bug.
