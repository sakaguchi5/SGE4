# Spiral 5 CU3 mutation matrix

The executable test mutates every Raw authority field individually and requires rejection. It also repeats selected attacks after recomputing the Raw Candidate identity.

## Raw proposal mutations: 40

| Group | Mutated fields |
|---|---|
| Semantic and schedule | Temporal Semantic identity, Update Schedule identity, invocation authority identity |
| Verification context | Target profile, Observation contract, History Resource contract, retained completion contract, device-epoch policy |
| Actual artifact chain | Spiral 4 indirect artifact, Argument Producer Program, Hierarchy Program, Consumer Program, CU2 artifact, proposed History Resource |
| Policies | Candidate kind, extension strategy, History Role, completion policy, generation policy, Hold execution policy |
| Dimensions | timeline, Work count, thread group, History depth/bytes, Local Motor bytes, argument stride/count, Producer/Hierarchy/Consumer Dispatch values |
| Schedule summary | Update count, Hold count, maximum source generation |
| Identity integrity | corrupted Raw identity |
| Rehashed attacks | wrong History Role, wrong Update count, wrong invocation authority, wrong CU2 artifact identity |

## Seal and Frozen replay gates

- P4 Verified seal replayed against P64,
- altered Target profile,
- altered History Resource contract,
- altered device-epoch policy,
- wrong actual History Resource identity,
- wrong History Role,
- wrong History byte size,
- zero device epoch,
- epoch-1 Frozen History replayed as epoch 2,
- P4 Frozen History replayed against P64.

All must fail before physical execution.
