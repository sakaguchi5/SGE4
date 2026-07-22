# Spiral 4 CU5 evidence ledger

| Evidence | Required result |
|---|---|
| full WARP corpus | 7 × 19 = 133 cases |
| active output | independent reference match in every case |
| numeric observation | absolute, relative, ULP, non-finite, homogeneous-coordinate metrics |
| Work structure | duplicate, missing, reordered, and out-of-range counts remain zero |
| inactive tail | 133 sentinel proofs |
| Batch argument records | 5 × 19 = 95 GPU/readback proofs |
| pairwise semantics | seven output digests equal for each `Nf` |
| native DeviceDomain | A/B/C execute on one supplied D3D12 device |
| Architecture determinism | Debug A, Debug B, Release bytes identical |
| controlled Recovery | epoch 1→2; native Device then post-rebind execution-object rematerialization |
| stale handle | rejected as `candidate-runtime/stale-epoch` |
| stale submission token | rejected as `candidate-runtime/stale-epoch` |
| missing rebind | rejected as `candidate-runtime/rebind-required` |
| post-Recovery observation | output and Batch records identical |
| actual removal | real `ID3D12Device5::RemoveDevice` failure reason |
| old-process quarantine | AwaitingAdapter, removed LUID excluded |
| removed-domain submissions | rejected as `candidate-runtime/device-state` |
| fresh rematerialization | same Frozen family and observation in new process |
| Runtime policy | dispatch and Candidate decision remains `None` |

Actual-removal DRED counts and HRESULT are physical diagnostics and are not
included in byte-identical canonical evidence.
