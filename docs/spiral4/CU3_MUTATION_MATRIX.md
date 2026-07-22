# Spiral 4 CU3 mutation matrix

The authority test starts from one canonical Planner proposal and verifies that
all of the following invalid proposals are rejected.

| # | Mutation |
|---:|---|
| 1 | Semantic identity |
| 2 | Target profile identity |
| 3 | Active Count contract identity |
| 4 | Observation contract identity |
| 5 | Command Signature contract identity |
| 6 | Argument Producer Program-template identity |
| 7 | Consumer Program-template identity |
| 8 | proposed sidecar artifact identity |
| 9 | Candidate kind |
| 10 | maximum Work count |
| 11 | thread-group size |
| 12 | Work record stride |
| 13 | output record stride |
| 14 | argument stride |
| 15 | argument offset |
| 16 | maximum command count |
| 17 | producer dispatch X |
| 18 | producer dispatch Y |
| 19 | producer dispatch Z |
| 20 | Command Signature kind |
| 21 | Operation code |
| 22 | active-range policy |
| 23 | state-transition policy |
| 24 | zero-work policy |
| 25 | Raw Candidate identity |
| 26 | maximum Work count with attacker-recomputed Raw identity |
| 27 | argument stride with attacker-recomputed Raw identity |

Cases 26 and 27 prove that a self-consistent reserialized Raw proposal is still
only a proposal. The independent Verifier's rederivation, not the Raw digest,
is authoritative.

Additional replay gates reject:

- cross-Target Verified certificate use,
- cross-Command-Signature Verified certificate use,
- cross-Observation Freeze use.
