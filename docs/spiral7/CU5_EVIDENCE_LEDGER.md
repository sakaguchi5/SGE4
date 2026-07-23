# Spiral 7 CU5 evidence ledger

## Accepted exhaustive evidence

| Evidence | Accepted SHA-256 | Result |
|---|---|---|
| Architecture Qualification | `1F1D09B4E52DBC35E961E0D3751B32292B2C30EEDCFA473800C5BB8E8CB5AB73` | Debug/Release byte-identical |
| Controlled Recovery | `7F0247B193F9BC20F4EDA5FEED590C1DA5722EF36B133640292B1ED616CFF62B` | Debug/Release byte-identical |
| Fresh rematerialization | `091EAEC0D27287FE897F6813043FD75C090D5DA17054461DF314F99B2A6F5A92` | Debug/Release byte-identical |

Accepted base commit: `67cb40b5204e1e06ecac576206ba969ec2db02b6`.

## Proof obligations

| Evidence | Required result | Routine enforcement | Exhaustive enforcement |
|---|---|---|---|
| Portable authority | 128 invocations and 384 Verified Candidate artifacts | Full Release | Full Debug and Release |
| Debug execution boundary | Representative A/B/C WARP execution | Four-invocation Debug smoke | Complete Debug qualification |
| Architecture qualification | 128 chained invocations and 384 Candidate executions | Full Release plus frozen digest | Full Debug/Release byte comparison plus frozen digest |
| Controlled Recovery | Epoch increment, stale-handle rejection, explicit `A_t/M_t` rebind and exact-generation seed | Full Release plus frozen digest | Full Debug/Release byte comparison plus frozen digest |
| Actual removal | RemoveDevice quarantine and removed-LUID exclusion | Full Release | Full Debug |
| Fresh process | Same Frozen authority and representative rematerialization | Full Release plus frozen digest | Full Debug/Release byte comparison plus frozen digest |
| Legacy boundary | No Schema 17, Runtime 17, Backend or Composition mutation | Static verifier | Static verifier |

The routine gate is not a smaller semantic corpus. It executes the complete corpus in Release and checks exact accepted evidence. Only the redundant complete Debug repetition is moved to the explicit exhaustive audit.
