# SGE4-5 Stage 03: Spiral 1 Research Contract Freeze

- Status: Frozen
- Foundation commit: `2e79dbf96255f1cac4dbb9fc133662123e78cf4a`
- Foundation solution SHA-256: `bb1ab5255967ed6ec1401128e5c4dfc1b522825a6161d0ab6a1a04d39e7ca826`
- Baseline archive: `SGE4v1.zip`
- Target ABI: D3D12 Schema 17 / Runtime 17
- Project count at freeze: 57

## 1. Stage 03の目的

Stage 03はGPU機能を追加する段階ではない。Spiral 1で後続実装が勝手に意味、誤差、候補、Verifier、Program interface、Project境界を変更できないよう、研究質問を実装可能な契約へ固定する段階である。

```text
F0: verified Level 4 v1 foundation
        ↓
Stage 03: research contract freeze
        ↓
Stage 04以降: frozen contractに従う実装
```

## 2. 固定されたResearch question

一つのCanonical PGA Motor剛体変換をMatrixへ早期消去せずCompiler candidate generationまで保持し、同一Semantic identityから`MatrixExpandedComputeV1`と`DirectPgaMotorComputeV1`を生成し、独立Verifier、Frozen Leaf、Level 4 v1 Buffer-only Composition、共通観測契約まで通せるか。

性能優位はArchitecture completionの条件ではない。

## 3. Stage 03の成果物

- Completion Specification v0.3
- Semantic Convention V1
- Observation Contract V1
- Corpus V1とmachine-readable definition
- Canonical Program Template Contract V1
- Authority / Project Boundary Contract V1
- Non-goals V1
- Proof Ledger V1
- Stage Map V1
- Contract Manifest V1
- F0 code baseline digest inventory
- Stage 03 verification runner

## 4. 権威順序

```text
Completion Specification v0.3
  ↓
Versioned split contracts
  ↓
CONTRACT_MANIFEST_V1.json
  ↓
Future implementation
```

実装が契約と衝突した場合、実装を修正する。契約を都合よく黙って変更しない。

## 5. 変更手続き

契約変更には次を必須とする。

1. 変更対象contractのversion更新
2. 変更理由
3. 破られる旧不変条件
4. ABI／Runtime／Observationへの影響
5. 新しいPositive scenario
6. 新しいNegative mutation
7. Proof Ledger更新
8. Contract Manifest再生成

## 6. Stage 03で禁止する変更

- `.cpp` / `.h` / `.vcxproj` / `.sln`の実行意味変更
- 新規空Projectの追加
- Schema 17 / Runtime 17の変更
- Texture、Temporal、Conditional、Variantなどの先行実装
- PGA固有概念のBackend／Runtimeへの導入
- 性能優位の宣言

## 7. Completion Gate

`run_sge4_5_stage03_contract.bat`が次を確認する。

- SGE4-5 identityとdependency boundary
- script encoding / syntax contract
- SOURCE_MANIFEST一致
- F0 code baseline全件一致
- 57 projectsとFoundation solution digestの維持
- 全contract fileの存在とdigest一致
- S1-I01からS1-I18のProof Ledger登録
- 未確定語、未version化contract、非ASCII pathがないこと

## 8. Completion banner

```text
============================================================
SGE4-5 STAGE 03 SPIRAL 1 RESEARCH CONTRACT FREEZE PASSED
Foundation commit: 2e79dbf96255f1cac4dbb9fc133662123e78cf4a
Foundation: Level 4 v1 / Schema 17 / Runtime 17 preserved
Research question: PGA rigid-transform representation retention
Lowering contracts: MatrixExpandedComputeV1 / DirectPgaMotorComputeV1
Observation: Corpus V1 and Observation Contract V1 frozen
Authority: Candidate, Verifier, Frozen Leaf and Composition boundaries frozen
Implementation: no Spiral 1 C++ feature code added in Stage 03
Ready for Stage 04 PGA Math and Canonical Semantic
============================================================
```
