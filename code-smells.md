# QuantLib Code Smells Audit

> Audit date: 2026-02-28
> Scope: `ql/` directory (2376 source files)
> Categories: Security · Performance · Modern C++ best practices

---

## 1. Security & Safety Issues

### 1.1 Swallowed Exceptions — Silent Failures in Financial Calculations

**Severity: CRITICAL**

Exceptions are silently caught and discarded in code that computes financial values. This can lead to silently incorrect pricing, which in a financial library is a **correctness and safety hazard**.

| File | Line | Pattern |
|------|------|---------|
| `ql/termstructures/volatility/sabr.cpp` | 345, 378 | `catch (Error&) {}` — SABR calibration silently ignores root-finding failure, returns uninitialized `alpha` |
| `ql/experimental/math/hybridsimulatedannealing.hpp` | 174 | `catch(...){ //Do nothing, move on to new draw }` |
| `ql/cashflows/lineartsrpricer.cpp` | 234 | `catch (...) { // use default value set above }` — solver failure silently falls back |
| `ql/termstructures/volatility/kahalesmilesection.cpp` | 116, 157, 210, 258 | Four separate `catch (...) {}` blocks in smile extrapolation logic |
| `ql/instruments/bonds/btp.cpp` | 231, 244 | `catch (...) { return false; }` — validity check swallows all exceptions |
| `ql/quotes/forwardswapquote.cpp` | 81 | `catch (...) {}` |
| `ql/termstructures/volatility/smilesection.cpp` | 134 | `catch(...) {}` |
| `ql/termstructures/volatility/gaussian1dsmilesection.cpp` | 104 | `catch (...) {}` |
| `ql/experimental/volatility/noarbsabrsmilesection.cpp` | 89 | `catch (...) {}` |
| `ql/cashflows/couponpricer.cpp` | 452 | `catch (...) {}` |

**Impact**: A SABR calibration failure in `sabr.cpp:345` silently returns garbage `alpha`, which flows into pricing. In financial software, silent wrong answers are worse than crashes.

### 1.2 `const_cast` Usage — Undermining Type Safety

**Severity: HIGH**

`const_cast` is used to mutate objects through `const` interfaces, violating const-correctness guarantees.

| File | Line | Code |
|------|------|------|
| `ql/termstructures/yield/fittedbonddiscountcurve.cpp` | 160 | `const_cast<FittedBondDiscountCurve*>(this)` |
| `ql/termstructures/iterativebootstrap.hpp` | 244 | `helper->setTermStructure(const_cast<Curve*>(ts_))` |
| `ql/experimental/credit/basket.cpp` | 103 | `lossModel_->setBasket(const_cast<Basket*>(this))` |
| `ql/termstructures/localbootstrap.hpp` | 164 | `const_cast<Curve*>(ts_)` |
| `ql/termstructures/globalbootstrap.hpp` | 343, 351 | `const_cast<Curve*>(ts_)` (twice) |
| `ql/termstructures/inflation/piecewisezeroinflationcurve.hpp` | 168 | `const_cast<this_curve*>(this)->baseDate_` |
| `ql/instruments/nonstandardswaption.cpp` | 28 | `const_cast<Swaption &>(fromSwaption)` |

**Impact**: If any of these objects are actually stored in read-only memory or shared across threads, undefined behavior results. The bootstrapping pattern (`setTermStructure(const_cast<Curve*>(ts_))`) is especially concerning as it's a core hot path.

### 1.3 Raw Pointer Members — Ownership Ambiguity

**Severity: HIGH**

Raw pointer class members without clear ownership semantics. Potential dangling pointer risk if the pointed-to object is destroyed.

| File | Line | Code |
|------|------|------|
| `ql/pricingengines/forward/forwardengine.hpp` | 61–62 | `mutable VanillaOption::arguments* originalArguments_; mutable const VanillaOption::results* originalResults_;` |
| `ql/termstructures/iterativebootstrap.hpp` | 113 | `Curve* ts_;` |
| `ql/termstructures/bootstraphelper.hpp` | 117 | `TS* termStructure_;` |
| `ql/termstructures/localbootstrap.hpp` | 61, 99 | `Curve* curve_; Curve* ts_;` |
| `ql/termstructures/globalbootstrap.hpp` | 143 | `Curve* ts_;` |
| `ql/math/optimization/levenbergmarquardt.hpp` | 62 | `Problem* currentProblem_;` |
| `ql/termstructures/volatility/abcdcalibration.hpp` | 66 | `AbcdCalibration* abcd_;` |
| `ql/patterns/lazyobject.hpp` | 135 | `LazyObject* subject_;` |

**Impact**: No smart-pointer or lifetime documentation. If the owning object is destroyed before these pointers are used, use-after-free occurs.

### 1.4 MINPACK C-style Interface — Raw Pointer Soup

**Severity: HIGH**

The MINPACK wrapper uses raw `Real*` pointers extensively throughout its API.

| File | Line | Code |
|------|------|------|
| `ql/math/optimization/lmdif.hpp` | 32–57 | `typedef std::function<void(int, int, Real*, Real*, int*)> LmdifCostFunction;` and all `lmdif`, `qrsolv`, `qrfac` signatures use raw `Real*`, `int*` parameters |

**Impact**: Buffer overruns, no bounds checking, no RAII. This is ported Fortran code where modern C++ types (`std::span`, `std::vector`) would be safer.

---

## 2. Thread Safety Issues

### 2.1 Pervasive `mutable` State — Thread-Unsafe Lazy Evaluation

**Severity: CRITICAL**

QuantLib uses a `LazyObject` pattern where `const` methods mutate `mutable` caches. Across **230+ files with 600+ `mutable` declarations**, objects that appear thread-safe (const interface) actually mutate internal state.

**Worst offenders** (objects likely shared across threads):

| File | Lines | Mutable members |
|------|-------|-----------------|
| `ql/instrument.hpp` | 107–109 | `mutable Real NPV_, errorEstimate_; mutable Date valuationDate_; mutable std::map<std::string, ext::any> additionalResults_;` |
| `ql/termstructure.hpp` | 105, 108 | `mutable bool updated_; mutable Date referenceDate_;` |
| `ql/indexes/indexmanager.hpp` | 60–61 | `mutable std::map<std::string, TimeSeries<Real>> data_;` — singleton with mutable map |
| `ql/currencies/exchangeratemanager.hpp` | 82 | `mutable std::map<Key, std::list<Entry>> data_;` — singleton with mutable map |
| `ql/patterns/lazyobject.hpp` | 131 | `mutable bool calculated_, frozen_, alwaysForward_;` |
| `ql/pricingengine.hpp` | 72–73 | `mutable ArgumentsType arguments_; mutable ResultsType results_;` |
| `ql/termstructures/interpolatedcurve.hpp` | 124–126 | `mutable std::vector<Time> times_; mutable std::vector<Real> data_; mutable Interpolation interpolation_;` |

**Impact**: Any two threads sharing an `Instrument`, `TermStructure`, or `IndexManager` singleton and calling `const` methods concurrently will have data races. `IndexManager` and `ExchangeRateManager` are **singletons** — sharing is inevitable in multi-threaded systems.

### 2.2 Singletons with Mutable Global State

**Severity: HIGH**

7 singleton classes hold mutable state accessible from any thread:

| Singleton | File |
|-----------|------|
| `Settings` | `ql/settings.hpp` |
| `IndexManager` | `ql/indexes/indexmanager.hpp` |
| `ExchangeRateManager` | `ql/currencies/exchangeratemanager.hpp` |
| `ObservableSettings` | `ql/patterns/observable.hpp` |
| `SeedGenerator` | `ql/math/randomnumbers/seedgenerator.hpp` |
| `CommoditySettings` | `ql/experimental/commodities/commoditysettings.hpp` |
| `UnitOfMeasureConversionManager` | `ql/experimental/commodities/unitofmeasureconversionmanager.hpp` |

`Settings::instance().evaluationDate()` is read/written pervasively across all pricing paths. Without external synchronization, any multi-threaded usage is unsafe.

### 2.3 Static Mutable Local Variables

**Severity: MEDIUM**

Static local variables with mutable initial values inside functions:

| File | Line | Code |
|------|------|------|
| `ql/experimental/barrieroption/perturbativebarrieroptionengine.cpp` | 98, 687, 750, 840, 842, 919, 1095, 1308 | Multiple `static double pi=...`, `static Real ONE=1.0, ZRO=0.0` etc. |
| `ql/pricingengines/barrier/analyticdoublebarrierbinaryengine.cpp` | 29 | `static Real PI= 3.14159265358979323846264338327950;` |

While C++11 guarantees thread-safe initialization of static locals, these are mutable (`double`, not `const double`), meaning they could theoretically be modified.

---

## 3. Performance Issues

### 3.1 `std::endl` Instead of `"\n"`

**Severity: MEDIUM**

`std::endl` forces a stream flush on every call. Found in **37 instances across 7 files**:

| File | Count |
|------|-------|
| `ql/models/shortrate/onefactormodels/markovfunctional.cpp` | 21 |
| `ql/experimental/commodities/energycommodity.cpp` | 2 |
| `ql/experimental/commodities/commodity.cpp` | 4 |
| `ql/experimental/commodities/commoditycashflow.cpp` | 4 |
| `ql/math/randomnumbers/sobolrsg.cpp` | 1 |

**Impact**: In the Markov functional model trace output (21 consecutive `std::endl` calls), this creates 21 unnecessary flush syscalls. Use `"\n"` and flush once at the end.

### 3.2 Massive `typedef` Count — 671 Across 204 Files

**Severity: LOW** (performance-neutral, but readability/maintenance cost)

The `types.hpp` file and 204 header files use `typedef` extensively (671 total). While not a performance issue per se, the core `types.hpp` defines all fundamental types as `typedef`:

```cpp
typedef QL_INTEGER Integer;
typedef QL_REAL Real;
typedef std::size_t Size;
typedef Real Time;
typedef Real DiscountFactor;
typedef Real Rate;
typedef Real Spread;
typedef Real Volatility;
typedef Real Probability;
```

All are `typedef` instead of `using`, though the clang-tidy config explicitly disables `modernize-use-using`.

### 3.3 `using namespace std` in Implementation Files

**Severity: MEDIUM**

Found in **18 `.cpp` files**:

| File |
|------|
| `ql/experimental/catbonds/catbond.cpp` |
| `ql/experimental/credit/randomdefaultmodel.cpp` |
| `ql/experimental/credit/onefactorcopula.cpp` |
| `ql/experimental/credit/syntheticcdo.cpp` |
| `ql/experimental/credit/cdo.cpp` |
| `ql/experimental/barrieroption/perturbativebarrieroptionengine.cpp` |
| `ql/experimental/credit/lossdistribution.cpp` |
| `ql/experimental/credit/basket.cpp` |
| `ql/experimental/callablebonds/blackcallablebondengine.cpp` |
| `ql/experimental/commodities/unitofmeasureconversionmanager.cpp` |
| `ql/pricingengines/asian/continuousarithmeticasianlevyengine.cpp` |
| `ql/pricingengines/exotic/analyticwriterextensibleoptionengine.cpp` |
| `ql/math/copulas/galamboscopula.cpp` |
| `ql/math/copulas/frankcopula.cpp` |
| `ql/math/copulas/gumbelcopula.cpp` |
| `ql/math/copulas/claytoncopula.cpp` |
| `ql/math/copulas/huslerreisscopula.cpp` |
| `ql/math/copulas/plackettcopula.cpp` |

**Impact**: Risk of name collisions. In a `.cpp` file this is less dangerous than in headers, but still considered bad practice.

---

## 4. Modern C++ Violations

### 4.1 `#define` Constants Instead of `constexpr`

**Severity: HIGH**

Numeric constants defined as preprocessor macros instead of typed `constexpr` values:

| File | Lines | Example |
|------|-------|---------|
| `ql/mathconstants.hpp` | 26–118 | 22 macros: `#define M_PI 3.14159...`, `#define M_E 2.71828...`, `#define M_SQRT2 1.41421...`, etc. |
| `ql/termstructures/volatility/kahalesmilesection.hpp` | 47–48 | `#define QL_KAHALE_SMAX 5.0`, `#define QL_KAHALE_ACC 1E-12` |
| `ql/math/solver1d.hpp` | 36 | `#define MAX_FUNCTION_EVALUATIONS 100` |
| `ql/termstructures/volatility/swaption/sabrswaptionvolatilitycube.hpp` | 47, 50 | `#define SWAPTIONVOLCUBE_VEGAWEIGHTED_TOL 15.0e-4`, `#define SWAPTIONVOLCUBE_TOL 100.0e-4` |
| `ql/math/integrals/exponentialintegrals.hpp` | 30 | `#define M_EULER_MASCHERONI 0.5772156649015328606065121` |

**Impact**: Macros have no type safety, no scope, pollute the global namespace, and cannot be debugged. `constexpr double` is strictly superior.

### 4.2 Unscoped `enum` Instead of `enum class`

**Severity: MEDIUM**

At least **30 unscoped enums** found in headers. Examples:

| File | Line | Code |
|------|------|------|
| `ql/option.hpp` | 39 | `enum Type { Put = -1, Call = 1 };` |
| `ql/time/frequency.hpp` | 37 | `enum Frequency { NoFrequency = -1, ... };` |
| `ql/time/date.hpp` | 57 | `enum Month { January = 1, ... };` |
| `ql/time/weekday.hpp` | 41 | `enum Weekday { Sunday = 1, ... };` |
| `ql/time/timeunit.hpp` | 37 | `enum TimeUnit { Days, ... };` |
| `ql/time/businessdayconvention.hpp` | 41 | `enum BusinessDayConvention { ... };` |
| `ql/prices.hpp` | 35, 68 | `enum PriceType { ... }; enum Type { Open, Close, High, Low };` |
| `ql/exchangerate.hpp` | 40 | `enum Type { Direct, ... };` |
| `ql/time/dategenerationrule.hpp` | 40 | `enum Rule { ... };` |

**Impact**: Unscoped enums implicitly convert to integers, pollute the enclosing namespace, and allow comparing values from different enums. `enum class` prevents all of these. Note: this is likely intentional for backward compatibility — but new code should use `enum class`.

### 4.3 C-style Casts

**Severity: MEDIUM**

C-style casts bypass type safety and can perform dangerous reinterpretations:

| File | Line | Code |
|------|------|------|
| `ql/methods/lattices/lattice.hpp` | 170 | `(long)this->impl().size(i)` |
| `ql/experimental/credit/defaultprobabilitylatentmodel.hpp` | 295, 309 | `(int)(poolSize)`, `(int)(n)` |
| `ql/experimental/inflation/yoycapfloortermpricesurface.hpp` | 415 | `(int)(maxSearchRange / searchStep)` |
| `ql/experimental/inflation/yoyinflationoptionletvolatilitystructure2.hpp` | 75 | `(int)ceil(...)` |
| `ql/termstructures/volatility/zabrsmilesection.hpp` | 186, 218 | `((double)j)`, `(long)strikes_.size()` |
| `ql/math/randomnumbers/zigguratgaussianrng.hpp` | 95 | `(int)(randomU64 & 0xff)` |

**Impact**: `static_cast<>` makes intent explicit and is searchable. C-style casts can silently perform `reinterpret_cast` when the programmer intended `static_cast`.

### 4.4 Hardcoded Magic Numbers — Pi Redefined Multiple Times

**Severity: MEDIUM**

Pi (and other constants) are redefined as `static double` local variables rather than using `M_PI` or `<numbers>`:

| File | Line | Value |
|------|------|-------|
| `ql/experimental/barrieroption/perturbativebarrieroptionengine.cpp` | 98 | `static double pi= 3.14159265358979324;` |
| same file | 687 | `static double ppi= 3.14159265358979324;` |
| same file | 750 | `static double ppi= 3.14159265358979324;` |
| same file | 842 | `static double ppi= 3.14159265358979324;` |
| same file | 1308 | `static double TWOPI = 6.283185307179586;` |
| `ql/pricingengines/barrier/analyticdoublebarrierbinaryengine.cpp` | 29 | `static Real PI= 3.14159265358979323846264338327950;` |

**Impact**: Inconsistent precision, multiple definitions, should use `std::numbers::pi` (C++20) or at minimum `M_PI` from `<cmath>`.

### 4.5 `new` Without `make_shared`/`make_unique`

**Severity: MEDIUM**

Raw `new` inside smart pointer constructors. While not a leak (smart pointer will manage lifetime), `make_shared` is more efficient (single allocation) and exception-safe:

| File | Line | Code |
|------|------|------|
| `ql/handle.hpp` | 82, 85 | `link_(new Link(p, registerAsObserver))` — this is the `Handle` class, one of the most-used types |
| `ql/experimental/commodities/petroleumunitsofmeasure.hpp` | 35–93 | 7 instances of `new Data(...)` |
| `ql/experimental/volatility/noarbsabrinterpolatedsmilesection.cpp` | 80–93 | `new SimpleQuote(...)` multiple times |
| `ql/models/model.cpp` | 33 | `new PrivateConstraint(arguments_)` |
| `ql/experimental/shortrate/generalizedhullwhite.hpp` | 241, 247 | `new GeneralizedOrnsteinUhlenbeckProcess(...)`, `new OrnsteinUhlenbeckProcess(...)` |

**Impact**: `ext::make_shared` (QuantLib's wrapper) is preferred for exception safety and performance. The project's `.clang-tidy` already configures `modernize-make-shared` to suggest `ext::make_shared`.

---

## 5. Exception Handling Issues

### 5.1 Broad `catch(...)` Without Re-throw or Logging

**Severity: HIGH**

18 instances of `catch(...)` in `.cpp` files that swallow **all** exceptions without any logging, re-throw, or error reporting:

| File | Line | Context |
|------|------|---------|
| `ql/termstructures/volatility/kahalesmilesection.cpp` | 116, 157, 210, 258 | Smile section extrapolation — silently retries |
| `ql/cashflows/lineartsrpricer.cpp` | 234, 265 | TSR pricing — uses fallback value on failure |
| `ql/instruments/bonds/btp.cpp` | 231, 244 | Validity check — returns `false` on any exception |
| `ql/experimental/credit/randomdefaultmodel.cpp` | 86 | Default model simulation |
| `ql/quotes/forwardswapquote.cpp` | 81 | Forward swap quote calculation |
| `ql/settings.cpp` | 65 | `SavedSettings` destructor — acceptable here |
| `ql/methods/finitedifferences/utilities/cevrndcalculator.cpp` | 120 | CEV random number calculation |
| `ql/cashflows/couponpricer.cpp` | 452 | Coupon pricing |
| `ql/patterns/observable.cpp` | 47, 83 | Observer notification — partially acceptable |

### 5.2 Empty `catch(Error&) {}` — SABR Calibration

**Severity: CRITICAL**

```cpp
// ql/termstructures/volatility/sabr.cpp:343-345
try {
    alpha = smallest_positive_root(c1, c2, c3, c4);
} catch (Error&) {}
// alpha remains uninitialized/previous value, returned to caller
```

This appears **twice** (lines 345 and 378) in the SABR parameter calibration. If root-finding fails, the stale `alpha` is returned and used in subsequent pricing calculations.

---

## 6. Code Quality & Maintainability

### 6.1 Excessive `mutable` — 600+ Declarations in 230 Files

The scale of `mutable` usage suggests the `LazyObject`/Observer pattern has grown beyond reasonable bounds. Particularly concerning:

- `ql/experimental/credit/` module: 20+ classes with 5-10 `mutable` members each
- `ql/termstructures/volatility/swaption/sabrswaptionvolatilitycube.hpp`: 8 mutable members including `Cube` objects
- `ql/experimental/inflation/yoycapfloortermpricesurface.hpp`: 14 mutable members

### 6.2 Redundant `static` Constants in Functions

The `perturbativebarrieroptionengine.cpp` file contains:
- `static Real ONE=1.0, ZRO=0.0` (line 840, 1095) — why not just use `1.0` and `0.0`?
- Multiple `static double pi=...` — should be `constexpr` at file scope

### 6.3 `typedef` in MINPACK Wrapper with Raw C API

```cpp
// ql/math/optimization/lmdif.hpp:32-36
typedef std::function<void (int, int, Real*, Real*, int*)> LmdifCostFunction;
```

This combines old-style `typedef` with raw pointers in the callback signature. A modern interface would use `std::span` or `std::vector&`.

---

## Summary

| Category | Severity | Count | Key Files |
|----------|----------|-------|-----------|
| Swallowed exceptions | CRITICAL | 18+ catch(...) blocks | sabr.cpp, kahalesmilesection.cpp, lineartsrpricer.cpp |
| Thread-unsafe mutable state | CRITICAL | 600+ mutable decls | instrument.hpp, termstructure.hpp, indexmanager.hpp |
| `const_cast` violations | HIGH | 9 instances | iterativebootstrap.hpp, globalbootstrap.hpp |
| Raw pointer members | HIGH | 15+ classes | forwardengine.hpp, iterativebootstrap.hpp, bootstraphelper.hpp |
| `#define` constants | HIGH | 50+ macros | mathconstants.hpp, kahalesmilesection.hpp |
| Broad catch blocks | HIGH | 18 `catch(...)` | across 12 files |
| Unscoped enums | MEDIUM | 30+ | option.hpp, date.hpp, frequency.hpp |
| C-style casts | MEDIUM | 13 instances | lattice.hpp, zabrsmilesection.hpp |
| `std::endl` overuse | MEDIUM | 37 instances | markovfunctional.cpp |
| `using namespace std` | MEDIUM | 18 .cpp files | experimental/ modules |
| `new` without `make_shared` | MEDIUM | 20+ | handle.hpp, various |
| Hardcoded Pi values | MEDIUM | 8 instances | perturbativebarrieroptionengine.cpp |
| `typedef` vs `using` | LOW | 671 | types.hpp, 204 files |
