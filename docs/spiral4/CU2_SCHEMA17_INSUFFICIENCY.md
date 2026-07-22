# Spiral 4 CU2 — Schema 17 insufficiency evidence

Baseline commit: `4033e8bf84650b6d1edbb6a8a83d97d5e1c3e4d1`

## Existing capability

The legacy D3D12 Package schema already contains the `IndirectArgument` Resource state. This allows a Buffer state transition to be named.

It does not contain:

- a Frozen Command Signature artifact,
- an indirect argument-kind record,
- an ExecuteIndirect or DispatchIndirect Operation code,
- an argument Buffer offset/stride/max-command contract,
- a Runtime record that can direct Backend to call `ExecuteIndirect`.

Therefore Schema 17 can express the Resource state but not the executable indirect command.

## Rejected escape hatch

The following implementation is forbidden:

```text
legacy ExecuteCompute operation
  -> Backend notices a special Buffer
  -> Backend reads or infers Active Count
  -> Backend chooses ExecuteIndirect
```

That would make Backend rediscover:

- direct versus indirect execution,
- Command Signature kind,
- dispatch argument layout,
- argument offset,
- maximum command count,
- zero-work behavior.

Those are Compiler decisions and must be Frozen.

## CU2 decision

CU2 does not mutate legacy Package Schema 17, Package Runtime 17, Composition Runtime, or the canonical D3D12 Backend.

It introduces a versioned sidecar extension artifact:

```text
SGE4-5.Spiral4.SingleIndirectArtifact.V1
```

The artifact explicitly fixes:

- Dispatch Command Signature,
- 12-byte `D3D12_DISPATCH_ARGUMENTS` stride,
- one maximum command,
- zero argument offset,
- one GPU Argument Producer dispatch,
- `Nmax = 4096`,
- `threadsPerGroup = 64`,
- UAV → INDIRECT_ARGUMENT state boundary,
- zero-work encoding by `groupCountX = 0`,
- Semantic and artifact identities.

The CU2 executor accepts only parsed canonical extension bytes and maps them mechanically to D3D12.

## Compatibility

The following legacy files must remain byte-unchanged in CU2:

- `10_D3D12PackageSchema/D3D12Schema.h`
- `10_D3D12PackageSchema/D3D12Encoding.cpp`
- `13_PackageRuntime/PackageRuntime.cpp`
- `13_PackageRuntime/PackageRuntime.h`
- `14_D3D12Backend/D3D12Backend.cpp`
- `14_D3D12Backend/D3D12Backend.h`

CU3 must decide whether the sidecar becomes a general Level 4 extension container or is integrated through a new canonical schema/runtime version. CU2 does not pre-commit that final layout.
