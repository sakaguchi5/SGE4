# Spiral 2 project boundaries v1

Projects 80-89 own production/experiment data flow; 90-99 own tests and the
launcher. The canonical direction is 80 semantic, 81 contracts/82 corpus, 83
raw candidate, 84 planner, 85 independent verifier, 86 Leaf compiler, 87
observer, 88 scenarios, 89 measurement.

Forbidden dependencies include Verifier to Planner implementation, Runtime or
Backend to semantic/candidate/planner/verifier, Frozen Leaf to source semantic,
Planner to Package/Runtime, Observer or CPU reference to Planner/candidate GPU
algorithms, Candidate A to B/C, and Direct PGA to hidden Matrix materialization.

Schema 17, Runtime 17, and Level 4 v1 are reused unless S2-09/S2-14 executable
evidence proves an exact insufficiency and triggers canonical reconstruction.

