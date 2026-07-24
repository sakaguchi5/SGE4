# Pre-v2 repository decision V1

## Decision

No tracked Spiral proof file is physically deleted in this preparation package.

The apparent duplication in per-CU change documents, preparation runners and package manifests is intentional input to the existing staged Verifiers. Deleting or moving those files now would weaken the reproducibility of the accepted reference implementation before v2 has a replacement migration gate.

## What was organized

- Spiral 7 status is changed from Owner-pending to closed.
- Real-GPU generated outputs are indexed as external evidence rather than copied into Git.
- A closure manifest and final Decision summary are added.
- `docs/level4v2` becomes the only planning entry point.
- A single static reference gate is added for normal pre-v2 work.
- Heavy CU5 and CU6 commands remain available only for relevant architecture or measurement changes.

## What is retired operationally

The following are historical-only, although still tracked:

- `run_sge4_5_spiral7_cu*_prepare.bat`
- per-CU application instructions
- changeset and package-layout documents
- stage-by-stage construction runners for CU1–CU4

They must not be used as the v2 development workflow.

## Future deletion point

Physical deletion is deferred to reconstruction R7, after v2 reproduces the relevant proof ledger and has a file-by-file retirement manifest.
