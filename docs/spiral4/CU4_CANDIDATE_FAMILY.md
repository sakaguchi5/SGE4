# Spiral 4 CU4 — verified Candidate family

Baseline commit: `04488a5203570d559ecd74372513d86abe34e4d0`

## Candidate corpus

CU4 holds the mathematical Work Semantic, Work records, Active Count,
transforms, output order, and Observation Contract constant.

It changes only physical Work issuance.

```text
A.FixedMaximumGuarded
  direct Dispatch for Nmax on every frame
  Consumer exits when index >= Nf

B.SingleExecuteIndirectDispatch
  one GPU-generated Dispatch record
  one ExecuteIndirect command

C.BatchedExecuteIndirectDispatch
  fixed slots of contiguous Active Work
  one root-constant FirstWork plus Dispatch per slot
```

C is a Candidate family:

```text
B64, B128, B256, B512, B1024
```

Therefore CU4 has seven verified Candidates.

## Why CU3 is preserved

CU3's V1 Single ExecuteIndirect types and SHA identities are not rewritten.

CU4 adds a Candidate-family V1 layer in projects 127–131. The existing CU3
single-candidate authority remains a regression oracle and a historical proof.

## Fixed-slot Batched execution

For Batch size `B`:

```text
maxBatchSlots = Nmax / B
firstWork(i) = i * B
workCount(i) = min(B, max(Nf - firstWork(i), 0))
groupsX(i) = ceil(workCount(i) / 64)
```

Every artifact has a fixed maximum command count. Empty slots encode
`groupsX = 0`. CU4 does not use an indirect count Buffer.

Each Batched argument record is 16 bytes:

```text
uint FirstWork
uint DispatchX
uint DispatchY
uint DispatchZ
```

The Command Signature sets Consumer root constant `FirstWork`, then issues
Dispatch.

## Level 4 and Level 5 split

Level 5 authority chooses and verifies:

- Fixed, Single, or Batched issuance,
- Batch size,
- maximum slots,
- argument record layout,
- Program-template identities.

Level 4 execution mechanically performs:

- direct Dispatch,
- Dispatch-only ExecuteIndirect,
- root-constant-plus-Dispatch ExecuteIndirect,
- required UAV and INDIRECT_ARGUMENT transitions.

Runtime does not select a Candidate, calculate group counts, split Batches, or
skip empty slots.
