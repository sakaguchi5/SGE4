# Spiral 7 CU5 test operation policy

## Decision

CU5 Architecture completion and full Debug/Release determinism were accepted from the Owner run based on commit:

```text
67cb40b5204e1e06ecac576206ba969ec2db02b6
```

The accepted evidence is:

```text
Architecture        1F1D09B4E52DBC35E961E0D3751B32292B2C30EEDCFA473800C5BB8E8CB5AB73
Controlled Recovery 7F0247B193F9BC20F4EDA5FEED590C1DA5722EF36B133640292B1ED616CFF62B
Fresh process       091EAEC0D27287FE897F6813043FD75C090D5DA17054461DF314F99B2A6F5A92
```

The former runner repeated the complete Debug audit on every invocation. The Owner timing evidence showed that Debug CPU authority construction dominated the gate, while Release completed the same evidence in seconds. Repeating the already accepted full Debug audit as the ordinary regression gate was therefore redundant.

## Routine gate

```powershell
.\run_sge4_5_spiral7_cu5_architecture_qualification.bat
```

The routine gate performs:

1. CU1–CU5 static and manifest verification.
2. Debug and Release builds.
3. A four-invocation Debug WARP smoke covering Initial, Hold, DirtyOnly and EmptyReset across A/B/C.
4. The complete 128-invocation portable authority test in Release.
5. The complete 128-invocation / 384-Candidate WARP Architecture Qualification in Release.
6. Controlled Recovery in Release.
7. Actual `ID3D12Device5::RemoveDevice` quarantine in Release.
8. Fresh rematerialization in Release.
9. Exact SHA-256 equality against the accepted exhaustive-audit evidence.

The routine gate does not weaken the qualified corpus. It changes only how frequently the complete Debug build repeats the same expensive authority reconstruction.

## Exhaustive determinism audit

```powershell
.\run_sge4_5_spiral7_cu5_exhaustive_audit.bat
```

This explicit audit repeats the complete Debug and Release portable, Architecture, Controlled Recovery and Fresh-process paths, requires byte-identical evidence, performs Debug RemoveDevice quarantine, and requires the accepted evidence SHA-256 values.

Run the exhaustive audit when the Architecture, evidence serializer, compiler/toolchain, Recovery contract, Device-epoch rules or relevant D3D12 execution code changes, or when the Owner requests a complete re-audit. It is not the default daily regression runner.

## Authority boundary

Neither runner selects A/B/C. Runtime Candidate-policy authorization remains `None`. Schema 17, Runtime 17, canonical Backend and Composition Contract remain unchanged.
