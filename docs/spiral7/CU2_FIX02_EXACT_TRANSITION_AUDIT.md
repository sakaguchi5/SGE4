# Spiral 7 CU2 FIX02 — Exact GPU Transition Audit

## Failure addressed

The original CU2 WARP qualification inferred that an Update occurred by requiring an updated survivor's output bytes to differ from previous history bytes.

That is not a valid authority proof: a legal Update may write a value byte-identical to the previous value.

## Correction

CU2 now allocates a separate 4096-entry GPU write-audit Buffer.

- `0`: no transition write,
- `1`: Update executed,
- `2`: Clear executed.

The Compute program writes the audit entry at the original universe index. Qualification requires:

- every member of `W_t` has audit value `1`,
- every member of `R_t` has audit value `2`,
- every index outside `T_t = W_t union R_t` remains `0`,
- retained output bytes remain byte-identical,
- cleared output bytes equal the canonical inactive sentinel,
- active output remains equal to the CPU semantic reference.

The Frozen Compact Delta artifact and all artifact identities remain unchanged. This fix changes only runtime observation and its static verifier tokens.

Failure output now includes the exact case identifier and every qualification flag.
