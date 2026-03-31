# HW2C Short-Rate Model Classification Analysis

Date: 2026-03-31
Context: Evaluating whether `HW2CModel` can be reformulated as a `OneFactorModel` or `TwoFactorModel` in QuantLib's short-rate model hierarchy.

---

## 1) Executive Summary

`HW2CModel` is **mathematically a one-factor model** and should be formulated as a `OneFactorModel` (specifically `OneFactorAffineModel`). Formulating it as a `TwoFactorModel` would be **mathematically incorrect**, computationally wasteful, and numerically degenerate. The current implementation as a bare `CalibratedModel` with two internal `HullWhite` instances is a pragmatic shortcut that misses the natural integration point and contributes to the day-counter bug documented in `hw2c-investigation.md`.

---

## 2) Financial Mathematics: What HW2C Actually Models

### 2.1 The Hull-White Short-Rate Process

The standard single-factor Hull-White model defines the short rate:

$$
dr_t = (\theta(t) - a \, r_t) \, dt + \sigma \, dW_t
$$

Equivalently, with the state variable $x_t$ (centered Ornstein-Uhlenbeck process):

$$
r_t = x_t + \varphi(t)
$$

where $x_t$ follows $dx_t = -a \, x_t \, dt + \sigma \, dW_t$ with $x_0 = 0$, and $\varphi(t)$ is the deterministic fitting function chosen so the model reprices the initial term structure exactly:

$$
\varphi(t) = f(0,t) + \frac{\sigma^2}{2a^2}(1 - e^{-at})^2
$$

### 2.2 The Two-Curve Extension

HW2C extends this to a **dual-curve framework** (e.g., EURIBOR forward projection + OIS/€STR discounting) while sharing a **single stochastic driver**:

- **Discount short rate:** $r_t^{\text{disc}} = x_t + \varphi_{\text{disc}}(t)$
- **Forward short rate:** $r_t^{\text{fwd}} = x_t + \varphi_{\text{fwd}}(t)$

where:

- $x_t$ is the **same** state variable in both expressions
- $\varphi_{\text{disc}}(t)$ is fitted to the discount curve
- $\varphi_{\text{fwd}}(t)$ is fitted to the forward curve
- Parameters $a$ and $\sigma$ are **shared** across both curves

### 2.3 Critical Observation: The Spread is Deterministic

The difference between the two rates:

$$
r_t^{\text{fwd}} - r_t^{\text{disc}} = \varphi_{\text{fwd}}(t) - \varphi_{\text{disc}}(t)
$$

is **purely deterministic** — it does not depend on the state variable $x_t$. This is the fundamental insight that determines the model's factor structure.

### 2.4 Factor Count: Unambiguously One

The model has:

- **One** source of randomness ($W_t$)
- **One** state variable ($x_t$)
- **Two** deterministic fitting functions ($\varphi_{\text{disc}}$, $\varphi_{\text{fwd}}$)

The number of stochastic factors is **one**. The two curves introduce no additional stochastic dimension — they only change the deterministic component of the short rate interpretation.

---

## 3) Can HW2C Be a `OneFactorModel`? — **Yes**

### 3.1 Contract Requirements

`OneFactorModel` (via `ShortRateModel`) requires:

| Virtual Method | Signature | HW2C Can Provide? |
|---|---|---|
| `dynamics()` | `→ shared_ptr<ShortRateDynamics>` | ✅ Yes — single OU process, one `shortRate(t, x)` |
| `tree()` | `→ shared_ptr<Lattice>` | ✅ Yes — single trinomial tree |

`ShortRateDynamics` requires:

| Method | Purpose | HW2C Mapping |
|---|---|---|
| `shortRate(t, x)` | State → short rate | $x + \varphi_{\text{disc}}(t)$ |
| `variable(t, r)` | Short rate → state | $r - \varphi_{\text{disc}}(t)$ |
| `process()` | Underlying OU process | $dx = -ax \, dt + \sigma \, dW$ |

The dynamics naturally maps to the **discount** curve. The `shortRate(t, x)` function returns the **discounting** short rate, which is correct because the lattice's `discount(i, j)` function uses this rate for backward induction (discounting cash flows).

### 3.2 `OneFactorAffineModel` — The Right Subclass

`OneFactorAffineModel` adds the affine bond pricing formula:

$$
P(t, T, r_t) = A(t,T) \, e^{-B(t,T) \, r_t}
$$

For HW2C:

- $B(t,T) = \frac{1 - e^{-a(T-t)}}{a}$ — **identical** for both curves (depends only on $a$)
- $A_{\text{disc}}(t,T)$ — computed from the discount term structure
- $A_{\text{fwd}}(t,T)$ — computed from the forward term structure

The base class's `A(t,T)` and `B(t,T)` naturally implement the discount curve. Forward curve bond pricing requires an **additional** `A_fwd(t,T)` method, but the affine structure is fully preserved.

### 3.3 Forward Rate Projection on a Single Tree

At tree node $(i, j)$ with state $x_j$ at time $t_i$, the forward projection zero-coupon bond is:

$$
P_{\text{fwd}}(t_i, T \,|\, x_j) = A_{\text{fwd}}(t_i, T) \cdot \exp\!\big(-B(t_i, T) \cdot (x_j + \varphi_{\text{fwd}}(t_i))\big)
$$

This is **analytically computable** at each node without a second tree. The forward LIBOR/EURIBOR rate for an index fixing at $t_s$ with maturity $t_e$ is:

$$
L(t_i; t_s, t_e \,|\, x_j) = \frac{1}{\tau}\left(\frac{P_{\text{fwd}}(t_i, t_s \,|\, x_j)}{P_{\text{fwd}}(t_i, t_e \,|\, x_j)} - 1\right)
$$

And the discounted floating coupon:

$$
\text{coupon}_j = N \cdot \tau \cdot \big(L(t_i; t_s, t_e \,|\, x_j) + s\big) \cdot P_{\text{disc}}(t_i, t_{\text{pay}} \,|\, x_j)
$$

Both $P_{\text{fwd}}$ and $P_{\text{disc}}$ are affine in $x_j$, computed analytically. **No second tree required.**

### 3.4 Advantages Over the Current Two-Tree Implementation

| Aspect | Current (Two Trees) | OneFactorModel (Single Tree) |
|---|---|---|
| Memory | 2× tree storage | 1× tree storage |
| Construction | Build + fit two trees | Build + fit one tree, precompute $A_{\text{fwd}}$ |
| Day-counter bug risk | Yes — separate rollback needs time-axis consistency | Eliminated — analytical formula uses only $(a, T-t)$ |
| Integration with QuantLib | None — bare `CalibratedModel` | Full — engines, calibration helpers, lattice methods |
| Analytical European options | Not available | Available via `discountBondOption()` |

### 3.5 Why the Day-Counter Bug Cannot Occur

The bug documented in `hw2c-investigation.md` arises because the two-tree approach requires computing `indexEndTimes` by adding an accrual period (in the floating leg's day counter) to a reset time (in the lattice's day counter). In the single-tree affine approach, forward bonds are computed as $A_{\text{fwd}}(t,T) \cdot e^{-B(t,T) r}$ where $T$ is always converted to the lattice time axis via a single consistent day counter. The accrual period $\tau$ appears only in the LIBOR formula's denominator, where it correctly uses the floating leg convention. There is no mixing of day-count conventions in time-axis arithmetic.

---

## 4) Can HW2C Be a `TwoFactorModel`? — **No**

### 4.1 What `TwoFactorModel` Represents

`TwoFactorModel` (exemplified by G2++) models the short rate as a function of **two independent stochastic state variables**:

$$
r_t = \varphi(t) + x_t + y_t
$$

where $x_t$ and $y_t$ follow **separate** OU processes with possibly correlated Brownian drivers:

$$
dx_t = -a \, x_t \, dt + \sigma \, dW_t^x, \quad dy_t = -b \, y_t \, dt + \eta \, dW_t^y, \quad dW_t^x \, dW_t^y = \rho \, dt
$$

The `ShortRateDynamics` contract requires:

| Method | Signature |
|---|---|
| `shortRate(t, x, y)` | $r = \varphi(t) + x + y$ |
| `xProcess()` | OU process for $x$ |
| `yProcess()` | OU process for $y$ |
| `correlation()` | $\rho$ |

The `tree()` method builds a **2D lattice** as the product of two independent trinomial trees.

### 4.2 Why HW2C Does Not Fit

| G2++ (True Two-Factor) | HW2C (Single Factor, Two Curves) |
|---|---|
| Two stochastic state variables $(x, y)$ | One stochastic state variable $x$ |
| Short rate depends on both: $r = \varphi + x + y$ | Short rate depends on one: $r = x + \varphi(t)$ |
| Parameters $(a, \sigma, b, \eta, \rho)$ — 5 free | Parameters $(a, \sigma)$ — 2 free |
| 2D lattice: $O(N^2)$ nodes per time step | 1D lattice: $O(N)$ nodes per time step |
| Correlation $\rho \in (-1, 1)$ is a modeling choice | Would require $\rho = 1$ (degenerate) |

### 4.3 The Correlation = 1 Degeneracy

If one were to force HW2C into `TwoFactorModel` by defining:

- $x_t$: state variable for discount curve
- $y_t$: state variable for forward curve

Then since both are driven by the **same** Brownian motion with the **same** $(a, \sigma)$:

$$
\rho = \text{Corr}(dW_t^x, dW_t^y) = 1
$$

A $\rho = 1$ two-factor model is **degenerate**:

- The 2D state space collapses to a 1D manifold ($y = x$ at all times, since $x_0 = y_0 = 0$ and the dynamics are identical)
- The 2D trinomial tree has $O(N^2)$ nodes but only $O(N)$ distinct states
- `TreeLattice2D` with $\rho = 1$ can produce numerically unstable branching probabilities
- The entire exercise wastes computation on a manifold that is trivially parameterized by a single variable

### 4.4 Financial Interpretation Error

Casting HW2C as `TwoFactorModel` would incorrectly imply that the discount and forward rates can move **independently** — that there exists a stochastic basis risk. HW2C explicitly models a **deterministic** basis:

$$
r_t^{\text{fwd}} - r_t^{\text{disc}} = \varphi_{\text{fwd}}(t) - \varphi_{\text{disc}}(t) = \text{deterministic}
$$

A `TwoFactorModel` with $\rho < 1$ would model:

$$
r_t^{\text{fwd}} - r_t^{\text{disc}} = (y_t - x_t) + \text{deterministic}
$$

where $(y_t - x_t)$ is stochastic — a fundamentally different and more complex model. If stochastic basis spread is desired, that is a valid modeling choice, but it requires different parameters $(b, \eta, \rho)$ and is a **different model** entirely, not a reformulation of HW2C.

---

## 5) Comparison with Existing QuantLib Precedents

### 5.1 `Gaussian1dModel` — The Dual-Curve One-Factor Pattern

`Gaussian1dModel` (`ql/models/shortrate/onefactormodels/gaussian1dmodel.hpp`) already provides the dual-curve pattern for one-factor models:

```cpp
Real numeraire(Time t, Real y = 0.0,
               const Handle<YieldTermStructure>& yts = Handle<YieldTermStructure>()) const;

Real zerobond(Time T, Time t = 0.0, Real y = 0.0,
              const Handle<YieldTermStructure>& yts = Handle<YieldTermStructure>()) const;
```

The optional `yts` parameter allows computing zero-coupon bonds against an **alternate** term structure (forward curve) while the model's primary term structure serves for discounting. This is exactly the HW2C use case.

`Gaussian1dModel::swapRate()` explicitly handles two-curve pricing:

- Floating leg: uses `zerobond(..., ytsf)` with the forward curve
- Fixed leg / discounting: uses `zerobond(..., ytsd)` with the discount curve

**Limitation:** `Gaussian1dModel` uses numerical integration (grid-based), not trinomial trees. It does not inherit from `OneFactorModel` and does not provide `dynamics()` or `tree()`. HW2C's tree-based approach would need the `OneFactorModel` / `OneFactorAffineModel` hierarchy instead.

### 5.2 `HullWhite` → `Vasicek` → `OneFactorAffineModel` Chain

The standard HullWhite model's inheritance:

```
CalibratedModel
  └── ShortRateModel
        └── OneFactorModel
              └── OneFactorAffineModel  (+AffineModel)
                    └── Vasicek
                          └── HullWhite  (+TermStructureConsistentModel)
```

Key contracts at each level:

| Class | Virtual Methods | Purpose |
|---|---|---|
| `ShortRateModel` | `tree(TimeGrid)` | Build lattice |
| `OneFactorModel` | `dynamics()` | Provide state-to-rate mapping + OU process |
| `OneFactorAffineModel` | `A(t,T)`, `B(t,T)` | Analytical bond pricing: $P = A e^{-Br}$ |
| `AffineModel` | `discountBondOption(...)` | Analytical European option pricing |

### 5.3 Recommended HW2C Inheritance

```
CalibratedModel
  └── ShortRateModel
        └── OneFactorModel
              └── OneFactorAffineModel  (+AffineModel)
                    └── HW2CModel  (+TermStructureConsistentModel)
```

With:
- `dynamics()` → OU process fitted to **discount** curve
- `tree()` → single trinomial tree (discount)
- `A(t,T)`, `B(t,T)` → discount curve affine factors
- Additional: `A_fwd(t,T)` for forward curve bond pricing
- Additional: `discountBond_fwd(t, T, r)` convenience method

---

## 6) Architectural Implications for the Reimplementation

### 6.1 What Changes vs. Current `HW2CModel`

| Current Design | Proposed Design |
|---|---|
| Inherits `CalibratedModel` | Inherits `OneFactorAffineModel` + `TermStructureConsistentModel` |
| Owns two `HullWhite` instances | Is itself a `HullWhite`-like model with two fitting parameters |
| `discountTree()` / `forwardTree()` | `tree()` (single, discount) |
| No `dynamics()` | `dynamics()` → discount-fitted `ShortRateDynamics` |
| No analytical bond pricing | `A(t,T)`, `B(t,T)` + `A_fwd(t,T)` for analytical pricing |
| Engines use two-tree rollback | Engines use single-tree rollback + analytical forward bonds |

### 6.2 Engine Design Impact

Discretized swap/swaption classes would change from:

**Current (two-tree):**
```
forwardBond → rollback on forward tree → read at node → compute forward rate
discountBond → rollback on discount tree → read at node → compute PV
```

**Proposed (single-tree, analytical):**
```
At node (i, j) with state x_j:
  r_disc = x_j + φ_disc(t_i)
  P_fwd(t_i, T) = A_fwd(t_i, T) · exp(-B(t_i, T) · (x_j + φ_fwd(t_i)))
  P_disc(t_i, T) = A(t_i, T) · exp(-B(t_i, T) · r_disc)
  L = (1/τ)(P_fwd(t_i, t_s) / P_fwd(t_i, t_e) - 1)
  coupon = N · τ · (L + s) · P_disc(t_i, t_pay)
```

This is simpler, faster, and eliminates the class of bugs arising from time-axis inconsistency between two separate trees.

### 6.3 Calibration

Unchanged conceptually:

- Calibrate $(a, \sigma)$ to market swaption volatilities
- $\varphi_{\text{disc}}(t)$ and $\varphi_{\text{fwd}}(t)$ are analytically determined from the initial curves
- `generateArguments()` recomputes both fitting parameters when $(a, \sigma)$ change

With analytical `discountBondOption()`, European swaption calibration becomes possible in closed form (faster than tree-based calibration), analogous to the standard `HullWhite` model.

---

## 7) Conclusion

| Question | Answer | Confidence |
|---|---|---|
| Is HW2C a one-factor model? | **Yes** — one stochastic driver, one state variable | High (mathematical proof) |
| Can HW2C be `OneFactorModel`? | **Yes** — fits the contract naturally | High |
| Can HW2C be `OneFactorAffineModel`? | **Yes** — affine bond pricing holds for both curves | High |
| Can HW2C be `TwoFactorModel`? | **No** — mathematically wrong, ρ=1 degenerate | High |
| Should HW2C inherit from `HullWhite` directly? | Possible but constrained — `HullWhite`'s constructor binds to one curve. A parallel sibling class under `OneFactorAffineModel` is cleaner. | Medium |
| Does `Gaussian1dModel` provide a precedent? | **Yes** — dual-curve one-factor pattern, but different engine paradigm (integration vs. trees) | High |

### Recommendation

Implement `HW2CModel` as a subclass of `OneFactorAffineModel` (and `TermStructureConsistentModel`), **not** as a `TwoFactorModel` and **not** as a bare `CalibratedModel`. This provides:

1. Correct mathematical classification
2. Full QuantLib infrastructure integration (calibration helpers, tree lattice, analytical pricing)
3. Elimination of the two-tree architecture and its associated bugs
4. Better computational performance (one tree, analytical forward bonds)
5. A clean, extensible design for dual-curve short-rate modeling

---

## Appendix A: Mathematical Proof That B(t,T) Is Curve-Independent

For the Hull-White model with mean reversion $a$:

$$
B(t, T) = \frac{1 - e^{-a(T-t)}}{a}
$$

This function depends only on $(a, T-t)$. Since HW2C uses the same $a$ for both curves, $B(t,T)$ is identical for discount and forward bond pricing. Only $A(t,T)$ — which involves the initial discount function $P^{\text{mkt}}(0,T)$ and forward rates $f(0,t)$ from a specific curve — differs between the two curves.

## Appendix B: Deterministic Forward-to-Discount Ratio

The ratio of forward to discount zero-coupon bonds at node $(i, j)$:

$$
\frac{P_{\text{fwd}}(t, T \,|\, x)}{P_{\text{disc}}(t, T \,|\, x)} = \frac{A_{\text{fwd}}(t,T)}{A_{\text{disc}}(t,T)} \cdot \exp\!\big(-B(t,T) \cdot (\varphi_{\text{fwd}}(t) - \varphi_{\text{disc}}(t))\big)
$$

This ratio is **independent of the state variable** $x$. It is a deterministic function of time only, precomputable before tree construction. This confirms that the two curves introduce no stochastic dimension — the basis is deterministic.

## Appendix C: QuantLib Class Hierarchy Reference

```
Observable
  ├── AffineModel (virtual)
  │     • discount(t), discountBond(t,T,factors), discountBondOption(...)
  │
  └── CalibratedModel (Observer + Observable)
        • calibrate(...), setParams(...), generateArguments()
        │
        └── ShortRateModel
              • tree(TimeGrid) = 0              ← pure virtual
              │
              ├── OneFactorModel
              │     • dynamics() = 0            ← returns ShortRateDynamics
              │     │
              │     └── OneFactorAffineModel    (+AffineModel)
              │           • A(t,T) = 0, B(t,T) = 0
              │           • discountBond(t,T,r) = A·exp(-B·r)
              │           │
              │           └── Vasicek
              │                 └── HullWhite   (+TermStructureConsistentModel)
              │
              └── TwoFactorModel
                    • dynamics() = 0            ← returns 2D ShortRateDynamics
                    │
                    └── G2                      (+AffineModel, +TermStructureConsistentModel)

Gaussian1dModel: TermStructureConsistentModel + LazyObject   (separate hierarchy)
```
