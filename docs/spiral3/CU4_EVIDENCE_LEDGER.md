# Spiral 3 CU4 evidence ledger

Scope: verified Leaf groups to unchanged Level 4 v1 Frozen Composition.

Implemented evidence:

- each R1/R4/R16/R64/R256/R512 experiment contains exactly eleven Leaves and twelve Buffer Flows;
- one Dynamic Motor source fans out to A/B/C and one immutable Point source fans out to all three Consumers, with exactly one writer per Flow;
- reference, A, B and C outputs fan in to one Observer and observation records are the sole Composition output;
- A uses Matrix materialization before hierarchy, B retains Motor through the Consumer, and C materializes Matrix after hierarchy;
- the verified plan contains 11 schedule entries, 12 allocations, 15 state handoffs, 11 signals and 15 waits;
- Runtime and Backend are absent from the CU4 scenario project; schedule, allocation, state and synchronization remain Frozen authority;
- role swaps, source swaps, duplicate writer, missing consumer, cross-R replay, digest and aggregate authority mutations are rejected;
- six Frozen Composition evidence records must be byte-identical across Debug A/B and Release;
- D3D12 Schema 17 / Runtime 17 remain unchanged.

Windows completion command: `run_sge4_5_spiral3_cu4_composition_sufficiency.bat`.
