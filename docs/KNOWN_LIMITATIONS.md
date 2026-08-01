# Known Limitations and Qualification Boundary

- Frozen Composition ABI 2.4のMSVC、HLSL、WARP、Actual Device removalは、このLinux環境では実行できない。最終合格はWindows上の`run_new_sge4_full_gate.bat`で確定する。
- `/std:c++latest`を維持するが、採用する設計機能は`std::expected`、`std::to_underlying`、compile-time Schema validation、限定的RangesなどC++23として選定した範囲に限る。
- Modules、`std::flat_map`／`std::flat_set`への一括置換、`std::mdspan`、`std::print`、`std::unreachable`、`[[assume]]`、複雑なViews pipelineは使用しない。
- Leaf Package ABIはSchema 17、Minimum Runtime 17を維持する。Frozen Composition ABI 2.4はLeaf bytesを完全に埋め込み、Leafを再符号化しない。
- Production Frozen Compositionは平坦な`SGE4UNI` major 2／minor 4である。内側の`SGE4CMP` Containerを持たない。
- `SGE4UNI 1.1`と`SGE4CMP 1.0`のReader／Writerはmigration資格試験用に`src/composition/migration/abi1/`へ隔離する。Production Runtimeは旧ABIを受理せず、自動upgradeも行わない。
- Dynamic Invocationは`SGE4INV` major 1／minor 4、Manifest schema 5であり、Execution Payload、Conditional Execution、Indirect Dispatch Sectionを必須とする。
- ABI 1.1から2.4へのMigrationではContract、Plan、Seal、Schedule、Recovery Set、Leaf bytesを保存する。Container bytes、file digest、Frozen Composition identity、Dynamic semantic identityは新ABIに従い変化する。
- Portable self-testは、手書きの有効なSchema 17 Leaf二個からABI 2.4を直接生成し、Round-trip、v1.1からのMigration、直接生成とのbyte一致、corruption rejectionを検査する。実HLSLから生成されるLeafとD3D12実行はWindows Full Gateで確定する。
- VerifiedDenseSlot modeではCompositionが対象Leaf、Dynamic Slot、member byte幅を固定し、SGE4INVがexact Update payloadをSealする。RuntimeはpayloadからMembershipを推測せず、Verifierが確定したUpdate／Clear transitionだけをprivate dense shadowへ適用する。AuthorityOnly modeは従来どおりpayloadを持たない。`SGE4INV`は前History identityをbindするが、任意の外部ストレージから`SGE4INV` bytesだけを再hydrateする公開Reader APIは追加していない。
- External rebindは、D3D12 Runtime再物質化後にCallerが明示的に承認するauthority gateである。任意の外部Asset systemを自動復元する一般機構ではない。
- Recovery unitはComposition全体であり、Partial Recoveryは未実装である。
- Bufferと限定Texture2Dによるfinite static DAG、single writer、optional single presenter、single adapterが現在のComposition範囲である。Texture2Dはfixed BGRA8、single mip／layer／plane／sample、same-frameだけを扱う。
- Texture2Dのmip／array／MSAA／UAV／Depth／subresource一般化、Frozen Variant Set、Streaming／Residency、Multiple Adapter、Runtime performance policyは含まない。Conditional Regionは非ネスト型、exact-set非空predicate、Leaf単位選択だけを扱う。任意bool slot、Conditional Presenter、branch固有Resource schemaは未実装である。これらの空SectionをABI 2.4へ先回りして追加していない。
- D3D12 Compiler、D3D12 Debug Layer、Driverが返すAPI固有の補足文字列は原文を含む場合がある。SGE自身が生成する人間向けメッセージは日本語とする。
- `reference/retired_source/`と`reference/retired_r5_runtime_reference/`は監査用であり、Active buildには含まれない。

- Generalization 4は一つのunconditional Compute Leaf／Compute Commandについて、exact transition countをD3D12 `ExecuteIndirect(DISPATCH)`へ接続する。Leaf schedule自体はFrozenのままである。
- Generalization 1は一つのDynamic universeを一つのLeaf Dynamic Slotへ写像する限定形である。複数Slot、複数Leafへのscatter、可変member byte幅は未実装である。

- Generalization 2は未選択LeafをSubmitせず、zero-Leaf submissionを許可する。Generalization 4のIndirect targetは初期版ではunconditional Leafに限定し、Conditional targetとの合成は行わない。
- 未選択Conditional Outputは直前に受理されたResource／completion状態を保持する。Runtimeはfallback生成や暗黙Clearを行わない。

- ABI 1.1 migration corpusはBuffer-onlyである。Texture2D Flowを旧形式から推測せず、Texture入力を明示拒否する。
- Texture upload／readbackのD3D12 row pitchはExecutorだけが所有し、Frozen Contract／Planはpacked rowBytesを所有する。

- Generalization 4のIndirect範囲はDispatch Xのみで、`X = workCount`、Y／Z=1、maxWorkCount=universeCountである。一般ExecuteIndirect、Draw indirect、複数target、Conditional target、GPU生成count、indirect chainは含まない。
