# Level 4 v2 R0 evidence ledger

| Evidence | Frozen property |
|---|---|
| `R0_INPUT_FREEZE_MANIFEST_V1.json` | Base commit, source identities, 40 carried invariants, owners and destinations |
| `R0_CARRIED_INVARIANT_MAP_V1.md` | Human-reviewable one-row-per-invariant migration map |
| `R0_CANONICAL_LAYER_OWNERSHIP_V1.md` | Single authoritative owner for each fact |
| `R1_CANONICAL_VOCABULARY_ENTRY_CONTRACT_V1.md` | R1 strong-type and public-name boundary before implementation |
| `Verify-Level4V2R0.ps1` | Source hash, uniqueness, owner, destination, forbidden-capability and no-project-mutation checks |
| Spiral 7 frozen reference gate | Closed reference program remains reproducible |
| `SOURCE_MANIFEST.sha256` | Applied package inventory after Owner preparation |

## Accepted R0 conclusion

```text
All carried facts have an owner.
All carried facts have an intended v2 destination.
No Runtime or Backend policy was added.
No implementation Project was added.
No reference Project was deleted.
R1 Canonical vocabulary may begin.
```

R0 contains no performance claim and does not reinterpret CU6 evidence.
