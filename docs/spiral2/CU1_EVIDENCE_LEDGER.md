# Spiral 2 CU1 evidence ledger

Stages: S2-03, S2-04, S2-05

- Canonical hierarchy: single root, bounded parent table, cycle rejection,
  root connectivity, arbitrary stable bone numbering, and deterministic
  `(depth,bone-index)` canonical order derived independently of numeric parent order
- Frozen hierarchy fields: bone count, topology, semantic identities and derived
  tables; no dynamic Motor values or backend representation concepts
- Dynamic invocation: binary32 `qr.wxyz + qd.wxyz`, 32 bytes per bone, exact slot
  size; Builder normalization for finite non-zero input rotations; strict unit,
  dual-orthogonality, canonical-zero, and shared `2^-52` sign validation at the
  serialized Palette boundary
- Authority: changing palette values preserves invocation/Frozen contract bytes;
  changing bone count or topology changes the schema/semantic identity
- Corpus: H00-H08 and F00-F11, 108 fixed combinations and five probes per bone
- CPU reference: independent binary64 dual-quaternion composition and point apply,
  rounded only at the common binary32 observation boundary
- Debug/Release/fresh-process semantic SHA-256:
  `49239990383190FC0438A9DE08CA096CC3F5B89AAC79EE5B6B20A89DA135832C`
- Debug/Release/fresh-process corpus SHA-256:
  `C2C656B3C963289AF2BDB4A2E5366758F78BF2394D51258EC14E9C6E3B0F8590`

```text
SelfAudit = Passed
IndependentSoftwareVerifier = RequiredForCU2
NextCapabilitySelection = DeferredByOwner
SelectionStatus = OWNER_DECISION_REQUIRED
```
