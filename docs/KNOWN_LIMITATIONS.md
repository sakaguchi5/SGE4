# Known Limitations and Qualification Boundary

- Frozen Composition ABI 2.1のMSVC、HLSL、WARP、Actual Device removalは、このLinux環境では実行できない。最終合格はWindows上の`run_new_sge4_full_gate.bat`で確定する。
- `/std:c++latest`を維持するが、採用する設計機能は`std::expected`、`std::to_underlying`、compile-time Schema validation、限定的RangesなどC++23として選定した範囲に限る。
- Modules、`std::flat_map`／`std::flat_set`への一括置換、`std::mdspan`、`std::print`、`std::unreachable`、`[[assume]]`、複雑なViews pipelineは使用しない。
- Leaf Package ABIはSchema 17、Minimum Runtime 17を維持する。Frozen Composition ABI 2.1はLeaf bytesを完全に埋め込み、Leafを再符号化しない。
- Production Frozen Compositionは平坦な`SGE4UNI` major 2／minor 1である。内側の`SGE4CMP` Containerを持たない。
- `SGE4UNI 1.1`と`SGE4CMP 1.0`のReader／Writerはmigration資格試験用に`src/composition/migration/abi1/`へ隔離する。Production Runtimeは旧ABIを受理せず、自動upgradeも行わない。
- Dynamic Invocationは`SGE4INV` major 1／minor 2、Manifest schema 3であり、Execution Payload Sectionを必須とする。
- ABI 1.1から2.1へのMigrationではContract、Plan、Seal、Schedule、Recovery Set、Leaf bytesを保存する。Container bytes、file digest、Frozen Composition identity、Dynamic semantic identityは新ABIに従い変化する。
- Portable self-testは、手書きの有効なSchema 17 Leaf二個からABI 2.1を直接生成し、Round-trip、v1.1からのMigration、直接生成とのbyte一致、corruption rejectionを検査する。実HLSLから生成されるLeafとD3D12実行はWindows Full Gateで確定する。
- VerifiedDenseSlot modeではCompositionが対象Leaf、Dynamic Slot、member byte幅を固定し、SGE4INVがexact Update payloadをSealする。RuntimeはpayloadからMembershipを推測せず、Verifierが確定したUpdate／Clear transitionだけをprivate dense shadowへ適用する。AuthorityOnly modeは従来どおりpayloadを持たない。`SGE4INV`は前History identityをbindするが、任意の外部ストレージから`SGE4INV` bytesだけを再hydrateする公開Reader APIは追加していない。
- External rebindは、D3D12 Runtime再物質化後にCallerが明示的に承認するauthority gateである。任意の外部Asset systemを自動復元する一般機構ではない。
- Recovery unitはComposition全体であり、Partial Recoveryは未実装である。
- Buffer-only finite static DAG、single writer、optional single presenter、single adapterが現在のComposition範囲である。
- Texture Flow一般化、Conditional Region、Frozen Variant Set、Streaming／Residency、Multiple Adapter、Runtime performance policyは含まない。これらの空SectionをABI 2.1へ先回りして追加していない。
- D3D12 Compiler、D3D12 Debug Layer、Driverが返すAPI固有の補足文字列は原文を含む場合がある。SGE自身が生成する人間向けメッセージは日本語とする。
- `reference/retired_source/`と`reference/retired_r5_runtime_reference/`は監査用であり、Active buildには含まれない。

- Generalization 1はDynamic Slotへ渡すdense bytesを実GPUへ接続するが、Leaf scheduleとDispatch数はまだ静的である。`indirectWorkCount`をExecuteIndirect／DispatchIndirectへ直接接続する能力は含まない。
- Generalization 1は一つのDynamic universeを一つのLeaf Dynamic Slotへ写像する限定形である。複数Slot、複数Leafへのscatter、可変member byte幅は未実装である。
