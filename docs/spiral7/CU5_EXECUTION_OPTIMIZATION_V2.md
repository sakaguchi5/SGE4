# Spiral 7 CU5 execution optimization V2

## Scope

This revision changes only the WARP execution mechanics used by CU4/CU5 qualification. It does not change:

- the Sparse–Temporal semantic,
- `A_t`, `M_t`, `W_t`, `H_t`, `R_t` or `T_t`,
- Candidate A/B/C artifacts or verification,
- legal write sets,
- report serialization,
- Architecture, Recovery or Fresh evidence formats,
- Runtime Candidate-policy authorization.

## Previous execution shape

The original executor created a fresh set of Upload, UAV and Readback resources for every Candidate and waited for one Fence after every Candidate.

For one 128-invocation Qualification run this meant:

```text
128 invocations x 3 Candidates = 384 Candidate dispatches
384 CommandList submissions
384 Fence waits
approximately 3,840 committed Buffer creations
```

Debug and Release, Recovery and Fresh rematerialization repeated the same allocation pattern.

## V2 execution shape

V2 creates one fixed-capacity reusable resource set for an execution context:

```text
one canonical Motor Upload
one Point Upload
one previous-History Upload
one zero-Audit Upload
three Candidate payload Uploads
three indirect-argument Uploads
three Output UAVs
three Write-Audit UAVs
three Output Readbacks
three Audit Readbacks
```

For each invocation:

1. current points and previous valid History are uploaded once;
2. A/B/C payloads and indirect arguments are refreshed;
3. all three Candidate outputs and audits are initialized;
4. A/B/C `ExecuteIndirect` calls are recorded in one CommandList;
5. all six UAVs are copied to Readback;
6. one submission and one Fence wait completes the invocation;
7. the same buffers are transitioned back and reused by the next invocation.

The Qualification shape becomes:

```text
384 Candidate dispatches                 unchanged
128 CommandList submissions              was 384
128 Fence waits                          was 384
22 committed Buffer resources     was approximately 3,840
```

The proof is not reduced. Every Candidate still executes for every invocation, every output and Write-Audit buffer is read back, and pairwise full-output equality remains checked after every invocation.

## Progress visibility

For timelines of at least 32 invocations, the executor prints progress every 16 invocations:

```text
[WARP-BATCH] invocations=16/128 candidateDispatches=48 fenceWaits=16
```

The PowerShell gate also prints the start, completion and elapsed time of every CU5 stage.
