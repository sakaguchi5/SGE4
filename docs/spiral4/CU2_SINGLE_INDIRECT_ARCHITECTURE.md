# Spiral 4 CU2 — Single ExecuteIndirect architecture

## Frozen path

```text
ActiveWorkSemanticV1
  Nmax = 4096
  threadsPerGroup = 64
  fixed Direct-PGA Consumer meaning
        ↓
FrozenSingleIndirectArtifactV1
  Command Signature = Dispatch
  argument stride = 12
  max command count = 1
        ↓
mechanical D3D12 execution
```

## Per-frame path

```text
CPU supplies dynamic Nf as an input Buffer
        ↓
GPU Argument Producer
  groupsX = ceil(Nf / 64)
  groupsY = 1
  groupsZ = 1
        ↓
UAV barrier
        ↓
UAV → INDIRECT_ARGUMENT transition
        ↓
ID3D12GraphicsCommandList::ExecuteIndirect
        ↓
Direct PGA point Consumer
        ↓
readback observation
```

The CPU writes `Nf`, but does not calculate or select the issued Dispatch dimensions. The only physical Consumer dispatch is read by D3D12 from the GPU-written argument Buffer.

## Qualified CU2 cases

```text
Nf = 0
Nf = 1
Nf = 63
Nf = 64
Nf = 65
Nf = 1024
Nf = 4096
```

For every case CU2 checks:

- observed argument `groupCountX`,
- expected `ceil(Nf / 64)`,
- active output against independent CPU translation reference,
- inactive output tail against a deterministic sentinel,
- full readback digest.

## Fixed experimental representation

CU2 fixes the Consumer representation to `DirectPgaThroughConsumerV1`. This isolates dynamic issuance from representation selection.

This is an experimental control, not a Runtime policy authorization and not a claim that Direct PGA is universally optimal.

## Authority limit

CU2 establishes explicit versioned bytes, canonical parsing, digest corruption rejection, and a mechanical executor boundary.

CU2 does not yet establish the final Level 5 authority chain. CU3 owns:

- separate Raw Candidate and Planner,
- Planner-independent Verifier,
- opaque context-bound Verified Plan,
- seal replay rejection,
- binding to actual Package/Shader/Operation digests.
