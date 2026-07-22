# Next capability selection from Spiral 4

## Owner decision

The Owner selects:

```text
Temporal State Flow and Verified History Reuse Lowering
```

for Spiral 5.

Baseline commit:

```text
f29d6597ec370d963c7b7dfbbc9af9590e8bd58f
```

## Evidence basis

Spiral 4 completed:

- seven verified physical Work-issuance Candidates,
- all 19 Active Count architecture cases,
- deterministic WARP and Recovery qualification,
- real-GPU measurement on NVIDIA GeForce RTX 4070 Laptop GPU,
- no observed Fixed/Single winner transition,
- no observed Single/BestBatch winner transition,
- Runtime policy authorization remained `None`.

The formal measurement evidence reported:

```text
Measurement evidence:
6C9B81AE04D2AD9EE0FA4043CCAA170E965B7D9FF2F1491E63EAE804D2CF5EAF

Decision report:
B786126B7249DFE36C1DECD04A2B20121149A2DCADB4460165689210E8EBD518

Measurement summary:
E61CB13B7120826B5F9534114D75A88EA57E02AA0E9881BBFF3EB09107E91E61
```

Spiral 4 showed that a Candidate family can be constructed, verified, recovered, and measured without returning policy to Runtime. It did not show that Active Count alone creates a useful adaptive boundary.

Spiral 5 therefore changes the structural variable from active quantity to temporal reuse.

## Selected question

```text
Given one exact piecewise-constant temporal meaning and one externally
verified update schedule, where should derived state be retained?
```

The compared boundaries are:

```text
A. no derived history reuse
B. history after hierarchy
C. history after the final Consumer
```

## Authority boundary

This selection does not authorize SGE to decide:

- game update rate,
- animation quality,
- distance or visibility policy,
- which frames are important,
- interpolation or approximation,
- Runtime adaptive Candidate selection.

The update schedule is supplied externally and verified. Hardware remains a Target/Measurement fact rather than part of the Semantic meaning.

## Closure status

The Owner decision closes Spiral 4’s next-capability gate and opens Spiral 5 CU1.

It does not pre-authorize any Spiral 5 implementation, ABI extension, Candidate winner, or Runtime policy.
