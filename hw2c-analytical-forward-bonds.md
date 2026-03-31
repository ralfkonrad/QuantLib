# Eliminating the Forward Tree via Analytical Forward Bond Pricing

Date: 2026-03-31
Status: Design analysis — not yet implemented
Depends on: `hw2c-shortrate.md` (mathematical foundation), Phase 1 fixes (completed)

---

## 1. Executive Summary

The current HW2C pricing engines build **two trinomial trees** — one fitted to the discount curve, one to the forward curve — and roll back `DiscretizedDiscountBond` objects on the forward tree to compute floating-leg coupons at each node. This is expensive, complex, and was the root cause of the day-counter bug documented in `hw2c-investigation.md`.

Because `HW2CModel` is now a `OneFactorAffineModel`, forward zero-coupon bonds can be computed **analytically** at each tree node using the affine formula:

$$
P_{\text{fwd}}(t, T \,|\, x) = A_{\text{fwd}}(t, T) \cdot \exp\!\big(-B(t, T) \cdot r_{\text{fwd}}(t, x)\big)
$$

where $r_{\text{fwd}}(t, x) = x + \varphi_{\text{fwd}}(t)$ and $x$ is the state variable at the node. This eliminates the forward tree entirely, along with the `HW2CDiscretizedAsset` dual-lattice interface, while preserving numerical accuracy.

---

## 2. Mathematical Foundation

### 2.1 Affine Bond Pricing in Hull-White

For a Hull-White model fitted to a term structure, the time-$t$ price of a zero-coupon bond maturing at $T$, conditional on the short rate $r_t$, is:

$$
P(t, T \,|\, r_t) = A(t, T) \cdot e^{-B(t, T) \cdot r_t}
$$

where:

$$
B(t, T) = \frac{1 - e^{-a(T-t)}}{a}
$$

$$
A(t, T) = \frac{P^{\text{mkt}}(0, T)}{P^{\text{mkt}}(0, t)} \cdot \exp\!\Big(B(t,T) \cdot f(0, t) - \frac{\sigma^2}{4a} B(t, T)^2 \big(1 - e^{-2at}\big)\Big)
$$

Here $P^{\text{mkt}}(0, \cdot)$ denotes initial market discount factors and $f(0, t)$ is the market instantaneous forward rate at time $t$.

### 2.2 Two-Curve Extension

HW2C shares a single stochastic driver $x_t$ with two fitting functions:

- **Discount**: $r_t^{\text{disc}} = x_t + \varphi_{\text{disc}}(t)$, fitted to the discount term structure
- **Forward**: $r_t^{\text{fwd}} = x_t + \varphi_{\text{fwd}}(t)$, fitted to the forward term structure

The fitting function is:

$$
\varphi_{\text{curve}}(t) = f_{\text{curve}}(0, t) + \frac{\sigma^2}{2a^2}\big(1 - e^{-at}\big)^2
$$

### 2.3 Key Invariant: B(t,T) Is Curve-Independent

$B(t, T)$ depends only on the mean-reversion speed $a$ and the time difference $(T - t)$. Since HW2C shares $a$ across both curves, $B(t, T)$ is **identical** for discount and forward bond pricing. Only $A(t, T)$ differs because it involves the curve-specific market discount factors and forward rates.

### 2.4 Forward Bond Pricing Formula

At tree node $(i, j)$ with state variable $x_j$ at time $t_i$:

$$
P_{\text{fwd}}(t_i, T \,|\, x_j) = A_{\text{fwd}}(t_i, T) \cdot \exp\!\big(-B(t_i, T) \cdot (x_j + \varphi_{\text{fwd}}(t_i))\big)
$$

where $A_{\text{fwd}}(t, T)$ uses the forward term structure's discount factors and instantaneous forward rates:

$$
A_{\text{fwd}}(t, T) = \frac{P_{\text{fwd}}^{\text{mkt}}(0, T)}{P_{\text{fwd}}^{\text{mkt}}(0, t)} \cdot \exp\!\Big(B(t,T) \cdot f_{\text{fwd}}(0, t) - \frac{\sigma^2}{4a} B(t, T)^2 \big(1 - e^{-2at}\big)\Big)
$$

This is **analytically computable** at each node without rollback.

### 2.5 Forward IBOR Rate at a Node

For a floating coupon with index fixing at $t_s$, index maturity at $t_e$, and index accrual fraction $\tau$:

$$
L(t_i; t_s, t_e \,|\, x_j) = \frac{1}{\tau}\!\left(\frac{P_{\text{fwd}}(t_i, t_s \,|\, x_j)}{P_{\text{fwd}}(t_i, t_e \,|\, x_j)} - 1\right)
$$

When evaluated at $t_i = t_s$ (the reset time), the numerator simplifies to $P_{\text{fwd}}(t_s, t_s | x_j) = 1$:

$$
L(t_s; t_s, t_e \,|\, x_j) = \frac{1}{\tau}\!\left(\frac{1}{P_{\text{fwd}}(t_s, t_e \,|\, x_j)} - 1\right)
$$

This matches the current implementation's formula (line 247 of `hw2cdiscretizedswap.cpp`):
```cpp
const Real forwardRate = (1.0 / projectionDiscount - 1.0) / spanningTime;
```

### 2.6 Discounted Floating Coupon at a Node

$$
\text{coupon}_j = N \cdot \tau_{\text{accrual}} \cdot \big(L(t_s; t_s, t_e \,|\, x_j) + s\big) \cdot P_{\text{disc}}(t_s, t_{\text{pay}} \,|\, x_j)
$$

Both $P_{\text{fwd}}$ and $P_{\text{disc}}$ are computed analytically from the same $x_j$.

---

## 3. Current Architecture (What Exists)

### 3.1 Dual-Lattice Interface

`HW2CDiscretizedAsset` (`hw2cdiscretizedasset.hpp`) defines a contract for discretized assets needing two lattices:

```cpp
class HW2CDiscretizedAsset {
    virtual const ext::shared_ptr<Lattice>& discountMethod() const = 0;
    virtual const ext::shared_ptr<Lattice>& forwardMethod() const = 0;
    virtual void initialize(const ext::shared_ptr<Lattice>& discountMethod,
                            const ext::shared_ptr<Lattice>& forwardMethod,
                            Time t) = 0;
};
```

`HW2CDiscretizedSwap` and `HW2CDiscretizedSwaption` both inherit from this.

### 3.2 Current Floating Coupon Computation

`HW2CDiscretizedSwap::addFloatingCoupon(i)` (lines 228–256):

1. Creates `DiscretizedDiscountBond discountBond` on the **discount** lattice, rolls back from `floatingPayTimes_[i]` to `time_`
2. Creates `DiscretizedDiscountBond forwardBond` on the **forward** lattice, rolls back from `indexEndTimes_[i]` to `indexStartTimes_[i]`
3. At each node $j$: computes forward rate from `forwardBond.values()[j]`, multiplies by discount bond, adds to `values_[j]`

The forward bond rollback requires the forward lattice to contain the mandatory times `indexStartTimes_` and `indexEndTimes_`, and it is this rollback that introduced the day-counter bug (fixed in this reimplementation by computing `indexEndTimes_[i]` using `dayCounter.yearFraction(referenceDate, args.floatingAccrualEndDates[i])` instead of `resetTime + spanning`).

### 3.3 Current Engine Flow

```
Engine::calculate()
  ├── Build timeGrid from mandatory times
  ├── discountLattice = model_->tree(timeGrid)        // discount tree
  ├── forwardLattice = model_->forwardTree(timeGrid)   // forward tree
  ├── swap.initialize(discountLattice, forwardLattice, maxTime)
  │     ├── DiscretizedAsset::initialize(discountLattice, t)
  │     └── forwardMethod_->initialize(*this, t)       // register with forward tree
  └── swap.rollback(0.0)
        └── at each reset time:
              addFloatingCoupon(i)
                ├── DiscretizedDiscountBond on discount tree → rollback
                └── DiscretizedDiscountBond on forward tree → rollback
```

### 3.4 What the Forward Tree Does

The forward tree (`model_->forwardTree(timeGrid)`) is a `ShortRateTree` built from the forward model's dynamics. Its sole purpose is to serve as the lattice for `DiscretizedDiscountBond` rollback in `addFloatingCoupon`. The **actual backward induction** (discounting of `values_`) always uses the discount tree.

---

## 4. Proposed Architecture (Analytical Forward Bonds)

### 4.1 New Model Methods

Add to `HW2CModel`:

```cpp
// Public:

/// Analytical forward-curve zero-coupon bond price.
/// Given the state variable x at time t, returns P_fwd(t, T | x).
Real forwardDiscountBond(Time t, Time T, Real x) const;

// Protected:

/// Forward-curve affine factor A_fwd(t, T).
Real A_fwd(Time t, Time T) const;
```

Implementation:

```cpp
Real HW2CModel::A_fwd(Time t, Time T) const {
    DiscountFactor discount1 = forwardTermStructure()->discount(t);
    DiscountFactor discount2 = forwardTermStructure()->discount(T);
    Rate forward = forwardTermStructure()->forwardRate(t, t, Continuous, NoFrequency);
    Real temp = sigma() * B(t, T);
    Real value = B(t, T) * forward - 0.25 * temp * temp * B(0.0, 2.0 * t);
    return exp(value) * discount2 / discount1;
}

Real HW2CModel::forwardDiscountBond(Time t, Time T, Real x) const {
    // r_fwd = x + φ_fwd(t)
    Rate r_fwd = forwardModel()->dynamics()->shortRate(t, x);
    return A_fwd(t, T) * std::exp(-B(t, T) * r_fwd);
}
```

Note: `forwardModel()->dynamics()->shortRate(t, x)` computes `x + φ_fwd(t)` because the forward model's dynamics use the forward fitting parameter. This reuses existing infrastructure.

**Alternative, self-contained approach** (avoids depending on `forwardModel()`): store `phi_fwd_` as a `Parameter` in `HW2CModel` (analogous to the existing `phi_` for discount), computed in `generateArguments()` as `HullWhite::FittingParameter(forwardTermStructure(), a(), sigma())`. Then:

```cpp
Real HW2CModel::forwardDiscountBond(Time t, Time T, Real x) const {
    Rate r_fwd = x + phi_fwd_(t);
    return A_fwd(t, T) * std::exp(-B(t, T) * r_fwd);
}
```

This is cleaner and doesn't depend on the internal `HullWhite` model instances.

### 4.2 Accessing the State Variable at Tree Nodes

The `DiscretizedAsset` framework provides `values_[j]` but not the state variable $x_j$ at each node. To compute analytical bonds, we need $x_j$.

**How to obtain $x_j$**: `ShortRateTree` (the lattice type returned by `model_->tree()`) exposes `underlying(i, index)` which returns the state variable at time step `i`, node `index`. The lattice is accessible via `method()` (inherited from `DiscretizedAsset`).

```cpp
auto shortRateTree = ext::dynamic_pointer_cast<OneFactorModel::ShortRateTree>(method());
Size i = cycleIndex;  // time grid index corresponding to time_
Real x_j = shortRateTree->underlying(i, j);
```

**Determining `i` from `time_`**: The discretized asset knows its current `time_` (set during rollback). The `TimeGrid` is accessible from the lattice. We need the index `i` such that `timeGrid()[i] ≈ time_`. This can be obtained via `timeGrid().index(time_)` or `timeGrid().closestIndex(time_)`, or stored during rollback.

**Practical approach**: Store the `ShortRateTree` pointer and time grid reference in `HW2CDiscretizedSwap` at initialization time, then look up the time index when needed.

### 4.3 Rewritten `addFloatingCoupon`

```cpp
void HW2CDiscretizedSwap::addFloatingCoupon(Size i) {
    const auto& tree = shortRateTree_;  // stored at initialization
    const Size timeIdx = tree->timeGrid().closestIndex(time_);

    const Time t = time_;
    const Time tEnd = indexEndTimes_[i];
    const Time tPay = floatingPayTimes_[i];

    const Real nominal = arguments_.nominal;
    const Time accrualTime = arguments_.floatingAccrualTimes[i];
    const Time spanningTime = fixingSpanningTimes_[i];
    const Spread spread = arguments_.floatingSpreads[i];

    for (Size j = 0; j < values_.size(); j++) {
        const Real x = tree->underlying(timeIdx, j);

        // Forward projection: P_fwd(t, t_end | x)
        const Real projectionDiscount = model_->forwardDiscountBond(t, tEnd, x);
        const Real forwardRate = (1.0 / projectionDiscount - 1.0) / spanningTime;

        // Discount to payment: P_disc(t, t_pay | x)
        const Rate r_disc = model_->dynamics()->shortRate(t, x);
        const Real paymentDiscount = model_->discountBond(t, tPay, r_disc);

        const Real couponAmount = nominal * accrualTime * (forwardRate + spread);
        const Real discountedCoupon = couponAmount * paymentDiscount;

        if (arguments_.type == Swap::Payer)
            values_[j] += discountedCoupon;
        else
            values_[j] -= discountedCoupon;
    }
}
```

**Key changes from current implementation**:
- No `DiscretizedDiscountBond forwardBond` — replaced by analytical `model_->forwardDiscountBond(t, tEnd, x)`
- The discount bond rollback (`discountBond.initialize/rollback`) can **also** be replaced by analytical `model_->discountBond(t, tPay, r_disc)` — this is a secondary optimization that also eliminates a rollback in `addFixedCoupon`
- All computation is local to each node — no inter-node rollback needed

### 4.4 Rewritten `addFixedCoupon` (Secondary Optimization)

The same analytical approach applies to fixed coupons. Currently:

```cpp
void HW2CDiscretizedSwap::addFixedCoupon(Size i) {
    DiscretizedDiscountBond discountBond;
    discountBond.initialize(discountMethod(), fixedPayTimes_[i]);
    discountBond.rollback(time_);
    // use discountBond.values()[j] at each node
}
```

Can become:

```cpp
void HW2CDiscretizedSwap::addFixedCoupon(Size i) {
    const auto& tree = shortRateTree_;
    const Size timeIdx = tree->timeGrid().closestIndex(time_);
    const Time t = time_;
    const Time tPay = fixedPayTimes_[i];
    const Real fixedCoupon = arguments_.fixedCoupons[i];

    for (Size j = 0; j < values_.size(); j++) {
        const Real x = tree->underlying(timeIdx, j);
        const Rate r = model_->dynamics()->shortRate(t, x);
        const Real discount = model_->discountBond(t, tPay, r);
        const Real coupon = fixedCoupon * discount;

        if (arguments_.type == Swap::Payer)
            values_[j] -= coupon;
        else
            values_[j] += coupon;
    }
}
```

This uses `OneFactorAffineModel::discountBond(t, T, rate)` which already exists and computes $A(t,T) \cdot e^{-B(t,T) \cdot r}$.

---

## 5. What Gets Eliminated

### 5.1 Files / Classes Removed

| Item | Location | Reason |
|---|---|---|
| `HW2CDiscretizedAsset` interface | `hw2cdiscretizedasset.hpp` | No dual-lattice contract needed |
| `forwardMethod_` member | `hw2cdiscretizedswap.hpp`, `hw2cdiscretizedswaption.hpp` | No forward lattice stored |
| `HW2CModel::forwardTree()` | `hw2cmodel.hpp/cpp` | No forward tree built |

### 5.2 Methods Simplified

| Method | Current | Proposed |
|---|---|---|
| `HW2CDiscretizedSwap::initialize()` | Dual-lattice init (discount + forward) | Standard single-lattice `DiscretizedAsset::initialize()` |
| `HW2CDiscretizedSwap::addFloatingCoupon()` | Forward bond rollback on forward tree | Analytical forward bond at each node |
| `HW2CDiscretizedSwap::addFixedCoupon()` | Discount bond rollback on discount tree | Analytical discount bond at each node |
| `HW2CDiscretizedSwaption::initialize()` | Stores forward lattice, inits discount | Standard single-lattice init |
| `HW2CDiscretizedSwaption::reset()` | Passes both lattices to underlying | Passes single lattice to underlying |
| `HW2CTreeSwapEngine::calculate()` | Builds two trees, dual-lattice init | Builds one tree, standard init |
| `HW2CTreeSwaptionEngine::calculate()` | Builds two trees, dual-lattice init | Builds one tree, standard init |

### 5.3 Constructor Changes

`HW2CDiscretizedSwap` and `HW2CDiscretizedSwaption` gain a reference/pointer to the `HW2CModel` (needed for `forwardDiscountBond()` and `discountBond()` calls). This replaces the forward lattice dependency.

### 5.4 Mandatory Times Reduction

Currently, `mandatoryTimes()` includes `indexStartTimes_` and `indexEndTimes_` because the forward bond rollback requires these times on the time grid. With analytical pricing, `indexEndTimes_` is no longer a mandatory time (we evaluate $P_{\text{fwd}}(t_s, t_e)$ analytically — we don't need $t_e$ on the grid). `indexStartTimes_` are still mandatory because `addFloatingCoupon` is triggered at reset times.

This means:
- `indexEndTimes_` can be **removed from `mandatoryTimes()`**
- The time grid becomes smaller, potentially improving performance
- `indexEndTimes_` is still needed as a member (used in the analytical formula) but doesn't need to be a grid point

Similarly, `fixedPayTimes_` and `floatingPayTimes_` could be removed from mandatory times if `addFixedCoupon` also goes analytical. However, pay times may still be needed as grid points for the `postAdjustValuesImpl` logic (in-the-past coupons added at pay time). This needs careful analysis.

---

## 6. What Stays

### 6.1 Preserved Without Change

| Component | Reason |
|---|---|
| `HW2CDiscretizedSwap` class (renamed/simplified) | Still needed for swap discretization logic |
| `HW2CDiscretizedSwaption` class (renamed/simplified) | Still needed for exercise logic and date-snapping |
| Date-snapping logic (`prepareSwaptionWithSnappedDates`) | Swaption exercise/coupon alignment unchanged |
| `CouponAdjustment` (pre/post) mechanism | Pre/post adjust logic unchanged |
| `postAdjustValuesImpl` for in-the-past coupons | Past-reset coupons still handled the same way |
| Constructor time computation logic | Reset times, pay times, accrual times still computed |
| `HW2CModel` class (extended) | Gains `A_fwd`, `forwardDiscountBond`; loses `forwardTree` |

### 6.2 `HW2CDiscretizedSwap` Inheritance Change

Currently:
```cpp
class HW2CDiscretizedSwap : public DiscretizedAsset, public HW2CDiscretizedAsset
```

Becomes:
```cpp
class HW2CDiscretizedSwap : public DiscretizedAsset
```

The `HW2CDiscretizedAsset` mixin is dropped entirely.

### 6.3 `HW2CDiscretizedSwaption` Inheritance Change

Currently:
```cpp
class HW2CDiscretizedSwaption : public DiscretizedOption, public HW2CDiscretizedAsset
```

Becomes:
```cpp
class HW2CDiscretizedSwaption : public DiscretizedOption
```

---

## 7. Impact on Engines

### 7.1 `HW2CTreeSwapEngine::calculate()` — Before

```cpp
void HW2CTreeSwapEngine::calculate() const {
    const Date referenceDate = model_->termStructure()->referenceDate();
    const DayCounter dayCounter = model_->termStructure()->dayCounter();

    HW2CDiscretizedSwap swap(arguments_, referenceDate, dayCounter);
    // ...
    const ext::shared_ptr<Lattice> discountLattice = model_->tree(timeGrid);
    const ext::shared_ptr<Lattice> forwardLattice = model_->forwardTree(timeGrid);

    swap.initialize(discountLattice, forwardLattice, maxTime);
    swap.rollback(0.0);
    results_.value = swap.presentValue();
}
```

### 7.2 `HW2CTreeSwapEngine::calculate()` — After

```cpp
void HW2CTreeSwapEngine::calculate() const {
    const Date referenceDate = model_->termStructure()->referenceDate();
    const DayCounter dayCounter = model_->termStructure()->dayCounter();

    HW2CDiscretizedSwap swap(arguments_, referenceDate, dayCounter, *model_);
    auto times = swap.mandatoryTimes();
    const Time maxTime = *std::max_element(times.begin(), times.end());

    TimeGrid timeGrid(times.begin(), times.end(), timeSteps_);
    const ext::shared_ptr<Lattice> lattice = model_->tree(timeGrid);

    swap.initialize(lattice, maxTime);   // standard single-lattice init
    swap.rollback(0.0);
    results_.value = swap.presentValue();
}
```

Changes:
- `HW2CDiscretizedSwap` constructor takes a `const HW2CModel&` reference
- Single tree built (`model_->tree()`), no `forwardTree()`
- Standard `DiscretizedAsset::initialize()` — no dual-lattice overload

### 7.3 `HW2CTreeSwaptionEngine::calculate()` — Analogous Simplification

Same pattern: single tree, model reference passed to `HW2CDiscretizedSwaption`, which passes it to its `HW2CDiscretizedSwap` underlying.

---

## 8. Day-Counter Bug Prevention by Construction

The day-counter bug (documented in `hw2c-investigation.md`) arose from mixing day-count conventions when computing `indexEndTimes_`:

```cpp
// OLD (buggy): resetTime uses lattice dayCounter, spanning uses floating-leg dayCounter
indexEndTimes_[i] = resetTime + spanning;
```

The current reimplementation already fixes this:

```cpp
// CURRENT (fixed): both use lattice dayCounter consistently
indexEndTimes_[i] = dayCounter.yearFraction(referenceDate, args.floatingAccrualEndDates[i]);
```

The analytical approach provides **additional protection**: `indexEndTimes_[i]` is used only as an argument to `forwardDiscountBond(t, T, x)` which evaluates $A_{\text{fwd}}(t, T)$ and $B(t, T)$ — both of which are functions of calendar time $T$, not of an accrual period. The accrual fraction $\tau$ (`spanningTime`) appears only in the LIBOR formula denominator where it correctly uses the floating-leg convention. There is **no arithmetic that mixes lattice times with accrual fractions**.

Even if `indexEndTimes_[i]` were computed incorrectly, the error would be confined to the projection discount factor $P_{\text{fwd}}(t, t_e)$ — it would not propagate through a lattice rollback that also depends on the time grid having $t_e$ as a grid point. The analytical formula is evaluated point-wise, so a time error in $T$ produces a proportional (not amplified) pricing error.

---

## 9. Implementation Plan

### Step 1: Add `A_fwd` and `forwardDiscountBond` to `HW2CModel`

- Add `phi_fwd_` member (analogous to `phi_`)
- Compute `phi_fwd_` in `generateArguments()` using `HullWhite::FittingParameter(forwardTermStructure(), a(), sigma())`
- Implement `A_fwd(Time t, Time T) const` using forward term structure
- Implement `forwardDiscountBond(Time t, Time T, Real x) const`
- Add unit tests: verify `forwardDiscountBond` matches `forwardModel()->discountBond()` for sample $(t, T, x)$ values

### Step 2: Rewrite `HW2CDiscretizedSwap`

- Add `const HW2CModel&` reference member and `ShortRateTree` pointer member
- Remove `HW2CDiscretizedAsset` base class
- Remove `forwardMethod_` member and `forwardMethod()` / `discountMethod()` virtual methods
- Replace dual-lattice `initialize()` with standard `DiscretizedAsset::initialize()`
- Store `ShortRateTree` pointer at initialization (via `dynamic_pointer_cast`)
- Rewrite `addFloatingCoupon()` to use `model_.forwardDiscountBond()` and `model_.discountBond()`
- Rewrite `addFixedCoupon()` to use `model_.discountBond()` (secondary optimization)
- Remove `indexEndTimes_` and `indexStartTimes_` from `mandatoryTimes()` (keep as members)
- Keep all existing pre/post adjustment logic unchanged

### Step 3: Rewrite `HW2CDiscretizedSwaption`

- Add `const HW2CModel&` reference member
- Remove `HW2CDiscretizedAsset` base class
- Remove `forwardMethod_` member
- Replace dual-lattice `initialize()` with standard `DiscretizedAsset::initialize()`
- Update `reset()` to pass model reference to underlying's single-lattice init
- Keep date-snapping logic unchanged

### Step 4: Simplify engines

- `HW2CTreeSwapEngine::calculate()`: build single tree, pass model to discretized swap
- `HW2CTreeSwaptionEngine::calculate()`: build single tree, pass model to discretized swaption
- Remove `HW2CModel::forwardTree()` once no callers remain

### Step 5: Remove `HW2CDiscretizedAsset`

- Delete `hw2cdiscretizedasset.hpp`
- Remove from all build system file lists (CMake, Autotools, VS project)

### Step 6: Evaluate whether internal `HullWhite` instances are still needed

After the forward tree is eliminated, the internal `forwardModel_` is used only for:
- `forwardDiscountBond()` if we take the `forwardModel()->dynamics()->shortRate(t, x)` route (but not if we use `phi_fwd_` directly)
- Nothing else in the engine path

The `discountModel_` is used for:
- `tree()` — delegates to `discountModel()->tree()`
- `discountBondOption()` — delegates to `discountModel()->discountBondOption()`

These delegations could eventually be inlined (implementing `tree()` and `discountBondOption()` directly in `HW2CModel`), but that is a separate future simplification. For now, the internal models remain useful for these delegations and for users who may want to inspect them.

---

## 10. Validation Strategy

### 10.1 Exact Equivalence Tests

The analytical approach must produce **identical** results to the current two-tree approach (to within floating-point tolerance) when both use the same time grid. This is because the analytical formula computes exactly the same mathematical object as the tree rollback — just without the discretization inherent in rolling back a `DiscretizedDiscountBond` through intermediate time steps.

Actually, there is a subtlety: the tree rollback of `DiscretizedDiscountBond` from $T$ to $t$ produces an **approximation** of $P(t, T)$ that depends on the time grid density between $t$ and $T$. The analytical formula is **exact**. So the two approaches will differ slightly, with the analytical approach being more accurate.

**Validation approach**: Show that the analytical result converges to the same limit as the two-tree result as the number of time steps increases, and that both converge to the same reference price.

### 10.2 Single-Curve Limit

When `discountTermStructure == forwardTermStructure`:
- `forwardDiscountBond(t, T, x)` must equal `discountBond(t, T, r)` where $r = x + \varphi(t)$
- HW2C swap prices must match standard `TreeSwapEngine` prices (existing test)
- HW2C swaption prices must match standard `TreeSwaptionEngine` prices (existing test)

### 10.3 Dual-Curve Spread Sensitivity

- Swap NPV must change when forward curve differs from discount curve (existing test)
- Swaption NPV must change with dual-curve spread (existing test)
- Forward rate at each node must be consistent with initial forward curve when $x = 0$

### 10.4 Convergence Tests

- Swap and swaption prices should converge as `timeSteps` increases
- Convergence rate should be at least as good as the two-tree approach (typically better, since there is no forward-bond rollback discretization error)

### 10.5 Regression Tests

- All 17 existing `HullWhiteWithTwoCurvesTests` must pass
- Numerical results may change slightly (improved accuracy from analytical vs. rollback) — update cached values if needed, with justification

### 10.6 BermudanSwaption Example as Integration Anchor

The `Examples/BermudanSwaption/BermudanSwaption.cpp` example provides an end-to-end integration test for the HW2C model: it calibrates to co-terminal European swaptions, then prices Bermudan swaptions at three strikes (ATM, OTM, ITM). In single-curve mode (forward curve = discount curve), the HW2C results must closely match the standard Hull-White numerical tree results, since the two models are mathematically equivalent in that limit.

**Reference values** (captured from current implementation, single-curve mode):

| | HW (numerical tree) | HW2C (current two-tree) | Δ |
|---|---|---|---|
| Calibrated $a$ | 0.046 | 0.061 | different calibration paths |
| Calibrated $\sigma$ | 0.005816 | 0.006173 | different calibration paths |
| ATM Bermudan | 13.145 | 13.153 | +0.008 |
| OTM Bermudan | 2.6157 | 2.6158 | +0.0001 |
| ITM Bermudan | 42.346 | 42.357 | +0.011 |

Note: HW and HW2C calibrate independently to the same co-terminal swaptions, so they reach slightly different $(a, \sigma)$ pairs — this is expected. The Bermudan swaption prices still agree to within ~1 bp in price, confirming the reimplementation is correct.

**After the Phase 2 refactor** (analytical forward bonds replacing the forward tree):

- The HW2C **calibration** may produce very slightly different $(a, \sigma)$ because the European swaption engine uses the same tree infrastructure. The analytical forward bond approach eliminates rollback discretization error in the forward bond, so calibration inputs are marginally more accurate.
- Bermudan swaption prices should remain within **0.1 bp** of the reference values above, or improve (move closer to the HW numerical tree values).
- If any price moves by more than **1 bp**, investigate — it likely indicates a bug in the analytical formula implementation.
- The example should be run as part of validation before and after the refactor, comparing output line-by-line.

---

## 11. Performance Considerations

### 11.1 Tree Construction

- **Before**: Two `ShortRateTree` instances built and fitted (2× tree construction, 2× fitting)
- **After**: One `ShortRateTree` instance

Savings: ~50% of tree construction time.

### 11.2 Per-Coupon Computation

- **Before**: Each `addFloatingCoupon` call creates a `DiscretizedDiscountBond`, initializes it on the forward tree, and rolls it back through multiple time steps (cost proportional to grid points between `indexStartTimes_[i]` and `indexEndTimes_[i]`)
- **After**: Each `addFloatingCoupon` call evaluates the analytical formula at each node (cost proportional to number of nodes at the current time step, with $O(1)$ per node)

The analytical approach is **strictly cheaper** per coupon because it avoids the multi-step rollback.

### 11.3 Memory

- **Before**: Two tree objects stored, forward lattice pointer in every discretized asset
- **After**: One tree object, model pointer in discretized assets

### 11.4 Time Grid

- **Before**: Time grid must include `indexStartTimes_` and `indexEndTimes_` as mandatory times (for the forward bond rollback)
- **After**: Time grid only needs `indexStartTimes_` (reset times) — `indexEndTimes_` are not grid points

Fewer mandatory times → coarser grid possible → faster rollback.

---

## Appendix A: `ShortRateTree` Interface Reference

From `ql/models/shortrate/onefactormodel.hpp`:

```cpp
class OneFactorModel::ShortRateTree : public TreeLattice1D<ShortRateTree> {
    Size size(Size i) const;                              // nodes at step i
    DiscountFactor discount(Size i, Size index) const;    // one-step discount
    Real underlying(Size i, Size index) const;            // state variable x
    Size descendant(Size i, Size index, Size branch) const;
    Real probability(Size i, Size index, Size branch) const;
};
```

The `underlying(i, index)` method returns the state variable $x$ at time step `i`, node `index`. This is the key method for computing analytical bonds at each node.

## Appendix B: `OneFactorAffineModel::discountBond` Reference

From `ql/models/shortrate/onefactormodel.hpp`:

```cpp
Real OneFactorAffineModel::discountBond(Time now, Time maturity, Rate rate) const {
    return A(now, maturity) * std::exp(-B(now, maturity) * rate);
}
```

This computes $P_{\text{disc}}(t, T \,|\, r)$ where `rate` is the **short rate** $r = x + \varphi_{\text{disc}}(t)$, not the state variable $x$. When calling from `addFloatingCoupon`, convert via `dynamics()->shortRate(t, x)`.

## Appendix C: Comparison of `addFloatingCoupon` — Current vs. Proposed

### Current (two-tree rollback):

```cpp
void HW2CDiscretizedSwap::addFloatingCoupon(Size i) {
    DiscretizedDiscountBond discountBond;
    discountBond.initialize(discountMethod(), floatingPayTimes_[i]);
    discountBond.rollback(time_);

    DiscretizedDiscountBond forwardBond;
    forwardBond.initialize(forwardMethod(), indexEndTimes_[i]);
    forwardBond.rollback(indexStartTimes_[i]);

    const Real nominal = arguments_.nominal;
    const Time accrualTime = arguments_.floatingAccrualTimes[i];
    const Time spanningTime = fixingSpanningTimes_[i];
    const Spread spread = arguments_.floatingSpreads[i];

    for (Size j = 0; j < values_.size(); j++) {
        const Real projectionDiscount = forwardBond.values()[j];
        const Real forwardRate = (1.0 / projectionDiscount - 1.0) / spanningTime;
        const Real couponAmount = nominal * accrualTime * (forwardRate + spread);
        const Real discountedCoupon = couponAmount * discountBond.values()[j];

        if (arguments_.type == Swap::Payer)
            values_[j] += discountedCoupon;
        else
            values_[j] -= discountedCoupon;
    }
}
```

### Proposed (analytical):

```cpp
void HW2CDiscretizedSwap::addFloatingCoupon(Size i) {
    const Size timeIdx = shortRateTree_->timeGrid().closestIndex(time_);

    const Time t = time_;
    const Time tEnd = indexEndTimes_[i];
    const Time tPay = floatingPayTimes_[i];

    const Real nominal = arguments_.nominal;
    const Time accrualTime = arguments_.floatingAccrualTimes[i];
    const Time spanningTime = fixingSpanningTimes_[i];
    const Spread spread = arguments_.floatingSpreads[i];

    for (Size j = 0; j < values_.size(); j++) {
        const Real x = shortRateTree_->underlying(timeIdx, j);

        // Forward projection bond
        const Real projectionDiscount = model_.forwardDiscountBond(t, tEnd, x);
        const Real forwardRate = (1.0 / projectionDiscount - 1.0) / spanningTime;

        // Discount to payment date
        const Rate r = model_.dynamics()->shortRate(t, x);
        const Real paymentDiscount = model_.discountBond(t, tPay, r);

        const Real couponAmount = nominal * accrualTime * (forwardRate + spread);
        const Real discountedCoupon = couponAmount * paymentDiscount;

        if (arguments_.type == Swap::Payer)
            values_[j] += discountedCoupon;
        else
            values_[j] -= discountedCoupon;
    }
}
```

Key differences:
1. No `DiscretizedDiscountBond` objects created
2. No `initialize` / `rollback` calls — O(1) per node instead of O(grid steps)
3. Forward bond computed analytically via `model_.forwardDiscountBond(t, T, x)`
4. Discount bond computed analytically via `model_.discountBond(t, T, r)`
5. State variable `x` accessed directly from the tree via `underlying(timeIdx, j)`
