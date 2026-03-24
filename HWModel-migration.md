# HWModelWithForwardAndDiscountCurve → master Migration Analysis

Date: 2026-03-24  
Repository: `QuantLib`  
Source branch: `HWModelWithForwardAndDiscountCurve`  
Target branch: `master`

---

## 1) Executive Summary

The `HWModelWithForwardAndDiscountCurve` branch implemented a **two-curve Hull-White (HW2C) experimental stack** (model + tree engines + tests) by introducing a dual-lattice valuation flow: one lattice for forward-rate projection and one lattice for discounting.

A direct merge into current `master` is **unsafe and not recommended** due to:
1. major upstream structural divergence since merge-base,
2. conflicting changes in `VanillaSwap` internals,
3. branch modifications to core discretized classes that are incompatible with a strict backward-compatibility policy on `master`.

**Recommended approach:** clean-room reimplementation on top of `master`, reusing branch intent and algorithms but avoiding invasive core API/ABI changes.

---

## 2) Probable Original Intent (Reconstructed)

### Primary goal
Add an experimental Hull-White model variant able to price swap/swaption under **separate forward and discount curves**, while keeping mean-reversion/volatility (`a`, `sigma`) common across both curves.

### Secondary goals
- Introduce tree engines for swap and swaption driven by two lattices.
- Preserve parity behaviors in known limiting cases (same discount and forward curve).
- Add tests covering model internals, swap valuation, swaption consistency, and calibration sanity.

### Confidence
**High**, supported by commit sequence, new experimental module layout, and test names/scenarios.

---

## 3) Strongest Evidence Trail

### Merge-base and branch ancestry
- Merge-base (`master` vs branch):  
  `d326db086a0e315172d1d10452bbd2ca06b70208`
- Branch head analyzed: `be1543250`
- Master head analyzed: `aab8c3da6`

### Branch evolution signals
- Non-merge commits on branch: **47**
- Active feature period: **2022-11 to 2023-04**
- Later branch activity mostly maintenance/hygiene (+ 2026 AGENTS change, unrelated)

### Diff footprint
- Files changed: **42**
- Approximate LOC delta: **+2038 / -113**

### Key implementation commits (feature-heavy window)
High-signal sequence includes:
`0fd21ba73`, `8166b7c98`, `c2193f3cf`, `0675d485a`, `d2ac5fdbc`, `39b75d1a0`, `c76f83b1b`, `c9539f570`, `9a3697370`, `3a5dbcfcb`, `62ac6247a`, `3cd7ac6eb`, `db958332b`, `a20210d9e`, `5a3f56764`, `5f8462f1f`, `8dfff6588`, `b1a663a4e`, `2968e3c27`, `d8d48c7c2`, `66b720bac`, `2a3c7dec9`, `ff0f3019c`, `e60a41223`, `99001919c`.

---

## 4) Branch Diff Overview (What Was Built)

## 4.1 New experimental module (self-contained core value)
Added under:
`ql/experimental/hullwhitewithtwocurves/`

### New model
- `model/hw2cmodel.hpp/.cpp`

`HW2CModel` encapsulates two internal `HullWhite` models:
- discount model tied to discount curve,
- forward model tied to forward curve,
sharing the same `a` and `sigma`.

### New discretized base
- `pricingengines/hw2cdiscretizedasset.hpp`

Defines dual-method initialization contract:
- discount lattice (`discountMethod`),
- forward lattice (`forwardMethod`).

### New engines/instruments
- swap: `hw2cdiscretizedswap.*`, `hw2ctreeswapengine.*`
- swaption: `hw2cdiscretizedswaption.*`, `hw2ctreeswaptionengine.*`

Core behavior: projected rates come from forward tree; discounting from discount tree.

### New tests
- `test-suite/hullwhitewithtwocurves.hpp/.cpp`

Coverage indicates intent to validate:
- model consistency,
- swap valuation mechanics,
- European/Bermudan swaption behavior,
- calibration-level sanity checks.

## 4.2 Branch modifications to existing core classes
Branch also changed existing non-experimental classes:
- `ql/instruments/vanillaswap.hpp/.cpp`
- `ql/pricingengines/swap/discretizedswap.hpp`
- `ql/pricingengines/swaption/discretizedswaption.hpp/.cpp`
- `ql/pricingengines/swaption/treeswaptionengine.cpp`
- `ql/discretizedasset.hpp`

Those changes are the main migration risk and merge blocker.

---

## 5) Why Direct Merge Is Unsafe

## 5.1 Structural conflict with modern `master`
`master` has refactored `VanillaSwap` around `FixedVsFloatingSwap` base-class architecture. The branch was built against older `VanillaSwap` internals and extends argument payload differently (`fixingValueDates`, `fixingEndDates`, `fixingSpanningTimes`).

Result: direct cherry-pick/merge causes semantic conflict, not just textual conflict.

## 5.2 Backward-compatibility constraint on `master`
Required project constraint: **changes on `master` must be backward compatible**.

Branch core changes include:
- widening encapsulation (`private` → `protected`) in discretized classes,
- introducing virtual dispatch on methods previously non-virtual,
- timeline plumbing differences in tree swaption flow.

These are high-risk for ABI and behavior stability on established master APIs.

## 5.3 Long-lived branch drift
Merge-base predates years of master evolution (1.38 → 1.42-dev era). High drift increases hidden integration defects in observer/lazy/discretization plumbing.

---

## 6) Compatibility Analysis Against Current Master

## 6.1 Directly reusable with adaptation
- Most files under `ql/experimental/hullwhitewithtwocurves/*`
- Most new tests in `test-suite/hullwhitewithtwocurves.*`

These represent the core feature value and can be ported with mechanical and API-adaptation edits.

## 6.2 Incompatible / should not be replayed as-is
1. `ql/instruments/vanillaswap.hpp/.cpp` branch argument-shape edits  
2. `ql/pricingengines/swap/discretizedswap.hpp` access/virtualization edits  
3. `ql/pricingengines/swaption/discretizedswaption.hpp/.cpp` access/timeline edits  
4. `ql/pricingengines/swaption/treeswaptionengine.cpp` workflow edits tightly tied to branch path

## 6.3 Potentially acceptable small master-side addition
- `ql/discretizedasset.hpp`: adding non-breaking accessor (`exerciseTimes()`) is likely feasible.

Evidence strength: **medium** (safe at source API level), but ABI policy/versioning decisions still require maintainer review.

---

## 7) Recommended Migration Strategy

### Strategy choice
**Clean-room reimplementation on top of `master` (recommended).**

### Rationale
- Preserves backward compatibility on `master`.
- Avoids invasive edits to mature core discretized classes.
- Reuses validated branch intent and formulas.
- Aligns with current architecture (not historical pre-refactor internals).

### Design principle
Treat branch as **specification and algorithm source**, not as patchset to replay.

---

## 8) Implementation Phases (Actionable Plan)

## Phase 1 — Port experimental module skeleton
- Recreate/port `ql/experimental/hullwhitewithtwocurves/*` on `master`.
- Keep public interfaces narrowly scoped and explicit.
- Compile-only checkpoint.

## Phase 2 — Rewire swap/swaption data extraction for modern `master`
- Adapt to `FixedVsFloatingSwap`-based argument flow.
- Do **not** depend on branch-only `VanillaSwap::arguments` extensions.
- Keep rate-fixing date-time computations local to HW2C components if possible.

## Phase 3 — Remove dependence on core visibility/virtuality changes
- Avoid requiring `DiscretizedSwap`/`DiscretizedSwaption` private→protected edits.
- Prefer composition or dedicated HW2C discretized classes owning required state.
- Only consider minimal shared utility extraction if backward-compatible and broadly useful.

## Phase 4 — Tree engine integration
- Implement dual-lattice initialization path in HW2C-specific engines.
- Maintain existing master engine behavior untouched for legacy engines.

## Phase 5 — Tests and numerical validation
- Port branch tests and update tolerances only where justified.
- Add regression checks for single-curve equivalence limit.
- Validate observer/lazy recalculation scenarios across relinked handles.

## Phase 6 — Build-system registration and CI parity
For every new file, update all required build systems:
- `ql/CMakeLists.txt`, `test-suite/CMakeLists.txt`
- corresponding `Makefile.am` sections
- Visual Studio project + filters (`QuantLib.vcxproj*`, `test-suite/testsuite.vcxproj*`)

Run file-list consistency checks (`tools/check_filelists.sh` path).

---

## 9) File-by-File Action Map

| File/Area | Action | Keep / Rewrite / Drop | Notes |
|---|---|---|---|
| `ql/experimental/hullwhitewithtwocurves/model/hw2cmodel.*` | Port with minor API adaptation | Keep (adapt) | Core concept is sound and self-contained. |
| `.../pricingengines/hw2cdiscretizedasset.hpp` | Port | Keep | Dual-method abstraction is central. |
| `.../swap/hw2cdiscretizedswap.*` | Port and adapt to master args | Rewrite partially | Remove reliance on branch core class edits. |
| `.../swap/hw2ctreeswapengine.*` | Port with constructor/interface alignment | Keep (adapt) | Ensure term-structure handle semantics follow master conventions. |
| `.../swaption/hw2cdiscretizedswaption.*` | Port and decouple from branch-only timeline assumptions | Rewrite partially | Keep algorithm, adapt plumbing. |
| `.../swaption/hw2ctreeswaptionengine.*` | Port with master-aligned exercise timeline handling | Keep (adapt) | Preserve existing master engine behavior for non-HW2C path. |
| `test-suite/hullwhitewithtwocurves.*` | Port and refresh expected values/tolerances if needed | Keep (adapt) | Keep economic invariants; adjust only with evidence. |
| `ql/instruments/vanillaswap.*` | Do not replay branch edits | Drop branch patch | Re-derive needed data in HW2C layer. |
| `ql/pricingengines/swap/discretizedswap.hpp` | Do not replay branch access/virtual changes | Drop branch patch | Backward-compatibility risk. |
| `ql/pricingengines/swaption/discretizedswaption.*` | Do not replay branch access edits | Drop branch patch | Backward-compatibility risk. |
| `ql/pricingengines/swaption/treeswaptionengine.cpp` | Do not replay wholesale | Drop branch patch | Implement behavior in HW2C-specific engine path. |
| `ql/discretizedasset.hpp` | Consider minimal accessor addition only if strictly needed | Optional targeted change | Must pass backward-compatibility review. |

---

## 10) Validation Strategy (Required Before Merge)

## 10.1 Build/test matrix
- CMake Linux build with tests on.
- Autotools build at least once.
- MSVC project inclusion and build files updated.
- `tools/check_filelists.sh` clean.

## 10.2 Functional checks
- HW2C swap valuation test set passes.
- HW2C European/Bermudan swaption tests pass.
- Single-curve limit (`forwardCurve == discountCurve`) aligns with existing one-curve behavior within tolerance.

## 10.3 Non-regression checks
- Existing swap/swaption test suites pass unchanged.
- No behavior drift in legacy engines.
- Observer/lazy recalculation and relink tests remain stable.

## 10.4 Backward compatibility checks
- No required API breaking changes in existing public classes.
- Avoid ABI-sensitive virtual-table/layout changes in mature core classes where possible.
- Any unavoidable master-core touch must be minimal, documented, and maintainer-approved.

---

## 11) Risks, Unknowns, and Assumptions

## 11.1 Risks
- Hidden dependency on old `VanillaSwap::arguments` shape.
- Subtle timeline differences in discretized swaption exercise handling.
- ABI sensitivity if core class visibility/virtuality is changed.

## 11.2 Assumptions
- Experimental module can remain isolated under `ql/experimental/`.
- Maintainers accept clean-room reimplementation over historical patch replay.
- Existing Gaussian1d dual-curve engine pattern is acceptable precedent for interface design.

## 11.3 Weak-evidence areas (explicit)
- Exact ABI impact tolerance depends on maintainers’ current release policy (not fully inferable from archaeology alone).
- Precise numeric tolerance migration effort cannot be finalized without rerunning full test matrix on target branch.

---

## 12) Engineer Checklist (Execution)

- [ ] Create migration branch from current `master`.
- [ ] Port `ql/experimental/hullwhitewithtwocurves/*` skeleton.
- [ ] Adapt HW2C swap/swaption discretized code to modern argument plumbing.
- [ ] Avoid branch replay of `VanillaSwap`/`DiscretizedSwap`/`DiscretizedSwaption` invasive edits.
- [ ] Add only minimal, backward-compatible core additions if strictly necessary.
- [ ] Port tests and refresh expected values only with evidence.
- [ ] Update CMake + Autotools + VS file lists/projects.
- [ ] Run targeted HW2C tests, then full impacted suites.
- [ ] Run file-list and CI-equivalent local checks.
- [ ] Prepare PR notes documenting backward-compatibility guarantees.

---

## 13) Recommended Execution Order

1. Port model + interfaces (`hw2cmodel`, dual-method base).  
2. Port swap discretization + tree engine and get compile green.  
3. Port swaption discretization + tree engine and compile.  
4. Port tests and fix numeric expectations.  
5. Add any minimal core helper accessors only if required and backward compatible.  
6. Run full validation matrix and finalize PR narrative.

---

## 14) Open Decisions Requiring Human Review

1. **Policy decision:** Is adding new public helper accessor(s) in core discretized assets acceptable under current ABI/API policy?  
2. **Scope decision:** Keep entirely in `experimental/` for first landing, or also expose additional public engine wiring immediately?  
3. **Testing decision:** Required breadth for non-default CI matrices before merge (especially non-default observer/session/thread configurations).  
4. **Numerics decision:** Acceptable tolerance bands for calibration-heavy tests across compilers/platforms.

---

## 15) Compact Console Summary

- merge-base: `d326db086a0e315172d1d10452bbd2ca06b70208`
- estimated feature commits: `~25` (core HW2C implementation window)
- top affected files:
  - `ql/experimental/hullwhitewithtwocurves/**` (new module)
  - `ql/instruments/vanillaswap.hpp/.cpp`
  - `ql/pricingengines/swap/discretizedswap.hpp`
  - `ql/pricingengines/swaption/discretizedswaption.hpp/.cpp`
  - `ql/pricingengines/swaption/treeswaptionengine.cpp`
- recommended migration strategy: **clean-room reimplementation on master with strict backward compatibility; do not directly merge/replay invasive core patches**.

---

## 16) Suggested `git worktree` Setup for Re-Implementation

This setup is intended to keep the current branch intact while implementing the migration from a fresh `master` checkout in a separate working directory.

### Why this layout fits this repository
- Existing local evidence already shows worktree metadata under `.git/worktrees/*` and a real detached worktree path (`.git/worktrees/master/gitdir` pointing to `.../QuantLib.worktrees/master/.git`).
- `CMakeUserPresets.json` defines `release` (inheriting from `default`) and `default` sets `"binaryDir": "${sourceDir}/build/${presetName}"`, so each worktree naturally gets isolated build artifacts under its own `build/` directory.

### Recommended directory layout
- Existing archaeology branch worktree (current):
  - `/home/rke/development/cpp/QuantLib`
- Existing master reference worktree (already used in analysis):
  - `/home/rke/development/cpp/QuantLib.worktrees/master`
- New implementation worktree (recommended):
  - `/home/rke/development/cpp/QuantLib.worktrees/hw2c-reimpl`

### Suggested commands
Run from the current repository root (`/home/rke/development/cpp/QuantLib`):

```bash
# 1) Ensure latest remote refs are available
git fetch origin

# 2) Create a fresh migration branch from origin/master in a dedicated worktree
git worktree add \
  "/home/rke/development/cpp/QuantLib.worktrees/hw2c-reimpl" \
  -b hw2c-reimplementation origin/master

# 3) Build inside that worktree using an existing preset
cmake --preset release \
  -S "/home/rke/development/cpp/QuantLib.worktrees/hw2c-reimpl"

cmake --build \
  "/home/rke/development/cpp/QuantLib.worktrees/hw2c-reimpl/build/release"
```

### Operational guidance
- Do all migration coding only in `hw2c-reimplementation` worktree.
- Keep the archaeology branch worktree unchanged for commit-by-commit reference.
- Keep `master` worktree as clean comparison baseline.
- Because changes on `master` must stay backward compatible, test and review core-touching changes in the new worktree before any PR.

### Optional cleanup (after merge or abandonment)
```bash
git worktree remove "/home/rke/development/cpp/QuantLib.worktrees/hw2c-reimpl"
git branch -D hw2c-reimplementation   # only if branch is no longer needed
```
