# Spiral 4 CU4 — Batch partition authority

## Frozen facts

A Batched Candidate freezes:

- Batch size,
- maximum Batch slots,
- 16-byte argument stride,
- maximum command count,
- producer dispatch size,
- root-constant-plus-Dispatch Command Signature.

## Runtime fact

Only `Nf` changes per invocation.

The Argument Producer maps `Nf` through the Frozen partition formula. It does
not discover arbitrary sparse Work and does not compact records.

## Independent proof

The Planner proposes the Candidate. The independent Verifier reconstructs the
artifact and partition policy without referencing the Planner project.

For each of five Batch sizes and all nineteen Active Count corpus values, CU4
derives and validates:

```text
firstWork is canonical and ascending
ranges do not overlap
ranges contain no gaps
union of nonempty ranges is exactly [0,Nf)
empty slots issue zero groups
all slots remain within Nmax
```

This produces 95 verified partition cases.

## GPU observation

For Nf 0, 65, and 4096, the GPU-generated Batch argument Buffer is copied to
readback and compared record-for-record with the independently derived
partition.

A correct final output is not sufficient by itself. Incorrect intermediate
Batch records are rejected even if a coincidental output matched.
