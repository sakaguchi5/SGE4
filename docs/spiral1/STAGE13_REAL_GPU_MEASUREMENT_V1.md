# Stage 13 Real GPU Measurement V1

- Base commit: `e5c8f864681a8ab4c143bb508c38af9fb01eaf03`
- Status: executable qualification
- Performance superiority is not a gate.

## Fixed profile

- Hardware D3D12 adapter only; WARP is rejected for performance claims.
- Cases: S03.0 (64), S15.15 (256), S07.0 (1024), S12.0 (4096).
- Warm-up: 200 samples per algorithm.
- Measurement: 2000 samples per algorithm.
- Repeats: 5.
- Order: ABBA blocks.
- Release build, one queue, one timestamp frequency.
- Modified counts remain valid only when embedded in the Benchmark Profile identity.

## Recorded evidence

Adapter description/LUID/vendor/device/driver, clock policy, timestamp frequency, corpus/Observation/target identities, Candidate/Verifier/Freeze times, Package/constant/upload/allocation/readback bytes, command recording, GPU dispatch and end-to-end median/p95, all raw samples and numeric Observation V2 metrics.

## Fairness boundary

The two lowerings use one input point buffer, the exact verified shader binaries, identical point count, dispatch shape, queue, root interface, warm-up, sample count and ABBA ordering. The experiment does not modify the Frozen architecture.
