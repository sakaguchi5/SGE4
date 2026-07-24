# SGE4-5 Spiral 7

## Capability

```text
Versioned Sparse Delta Flow and Verified Incremental History Lowering
```

Spiral 7 combines verified indirect quantity, temporal history and exact sparse membership over `A_t`, `M_t`, `N_t`, `R_t`, `W_t`, `H_t` and `T_t`.

## Final status

- CU1–CU5: PASSED
- CU6-1: PASSED — canonical real-GPU surface measured
- CU6-2: PASSED — paired Decision authority and high-Transition refinement measured
- Architecture: `SGE4-5 SPIRAL 7 ARCHITECTURE COMPLETE`
- Experiment: `SGE4-5 SPIRAL 7 EXPERIMENT COMPLETE`
- Owner closure: `SGE4-5 SPIRAL 7 CLOSED`
- Accepted final commit: `f802c12b162569a869c214da22b80142b3a4a0dd`

The Owner request to organize the repository before Level 4 v2 is the explicit closure action. No result authorizes Runtime Candidate selection or a universal performance winner.

## Candidate family

```text
A = FullActiveDenseRecompute
B = CompactDeltaIndexHistoryReuse
C = AffectedBlockDeltaHistoryReuse
```

## Accepted real-GPU evidence

```text
Adapter                    NVIDIA GeForce RTX 4070 Laptop GPU
Adapter fingerprint        8a7a825e84c0a157b16c6e54f51b833d6d191173e7e1d28d84062a60840a6b3f
Canonical evidence         0d7a50c448269cbb346ea80b0ceda09e7f28b10c068ed20a96d6c47621b4802a
Refinement evidence        44fa4dd72e64aab300bec53fc45c6ae2bf7a7012630b0e15a83fe6717c7ff80b
Combined decision CSV      e07af74f1f5f53143adbaf5e6fbd65d54d83bc4a0d314c6e7d648bf4edcd7bd4
Combined decision report   b840cb1425f5e45ccac2f0c84750584313350458062dab98f8552290e8bfda81
External evidence ZIP      9ee852c2a2574a8cb9c163b6e94213b855317bba74319e6443e9aa98c10c24d9
```

The evidence ZIP and generated `.bin/.csv/.txt` files are external build outputs and are not tracked by Git. Git records only their identities and interpretation.

## Final Decision Map

```text
Combined coordinates       220
ZeroDispatchEquivalent      20
StableWinner                41
StableEquivalentSet         61
Unresolved                  98
Stable winners              B only
B/C stable equivalent       59
A/B/C stable equivalent      2
```

All 41 stable single-winner coordinates selected B. They occur only in `UniformStride` and `HashScatterPermutation`, supporting affected-block spread as a material variable. No stable single-winner coordinate selected A or C. At high Transition counts A becomes competitive, but no universal or Adapter-independent winner is implied.

## Normal reference gate

```powershell
.\run_sge4_5_pre_level4v2_prepare.bat
.\run_sge4_5_spiral7_reference_gate.bat
```

The reference gate performs static lineage, authority, boundary and handoff verification. The CU5 WARP gate and CU6 real-GPU measurement remain available, but are not routine pre-v2 commands.

## Authority boundary

```text
RuntimeCandidatePolicyAuthorization = None
UniversalWinnerClaim = Forbidden
MeasurementResultScope = AdapterDriverAndMeasurementProfileSpecificObservation
NextProgram = Level4V2CanonicalReconstruction
```

The canonical reconstruction entry point is `docs/level4v2/README.md`.
