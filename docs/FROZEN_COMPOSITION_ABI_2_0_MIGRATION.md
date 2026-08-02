# Frozen Composition ABI 1.1 -> 2.0 Migration Contract

## 1. Oracle

`NewSGE4 v1.5.2 FULL GATE PASSED`をABI 1.1の変更不能Oracleとする。

```text
SGE4UNI 1.1
  CompleteComposition = SGE4CMP 1.0
```

ABI 1のReader／WriterはProduction互換層として残すのではなく、migration資格試験専用とする。

## 2. Migration経路

```text
SGE4UNI 1.1 bytes
  -> ABI 1 outer Section検証
  -> embedded SGE4CMP Reader
  -> Contract decode
  -> Leaf Schema 17検証
  -> Plan decode
  -> Independent Composition Verifier
  -> v1 Certificate／Authority／Dynamic binding照合
  -> Verified Contract + Verified Plan
  -> SGE4UNI 2.8 Freeze
```

MigratorはPlannerを再実行しない。旧Artifactに保存されたPlanを独立Verifierで再検証し、そのVerified PlanをABI 2.0へFreezeする。

## 3. 保存する意味

Migration前後で次を保存する。

- Schema 17 Leaf Package bytes
- Leaf stable key
- Contract identity
- Plan identity
- Verifier seal identity
- Schedule identity
- Recovery set identity
- Leaf count
- Flow count
- Dynamic universe count
- Compositionの実行意味
- WARP観測結果
- Recovery状態遷移

ABI 2.0は新しいContainerとDigest階層を持つため、次は一致を要求しない。

- SGE4UNI file bytes
- file digest
- semantic digest
- Frozen Composition artifact identity
- Dynamic semantic identity

ただし、同じ意味入力をProduction v2 Writerへ直接渡した結果と、v1 Corpusをmigrationした結果は、最終的なABI 2.0 bytesが完全一致しなければならない。

## 4. Production方針

```text
Production Runtime
  SGE4UNI 2.8のみ

Migration qualification
  SGE4UNI 1.1 -> SGE4UNI 2.8

Legacy ABI code
  src/composition/migration/abi1/ のみ
```

Runtimeでの自動upgrade、v1互換mode、v1／v2の暗黙判別は行わない。

## 5. 完成Gate

- v1.5.2 Golden bytesを生成できる
- Production Readerがv1.1を拒否する
- Migration Toolがv1.1を受理する
- v1のContract／Plan／Seal／Schedule／Recovery identityを検証する
- v2へ変換できる
- 直接生成v2とmigration v2がbyte一致する
- 変換後の全Leaf bytesが元と一致する
- v2 corruption corpusを通過する
- Debug A／Debug B／Releaseでv2 bytes一致
- WARP／Controlled Recovery／Actual Removalを通過する


## 2.1 note

ABI 1.1はauthority-only Dynamic Contractだけを表現できる。Migratorはこれをschema 6の`AuthorityOnly`へ明示変換し、直接生成したSGE4UNI 2.8とbyte一致させる。VerifiedDenseSlotは旧ABIから推測せず、新規Composition入力からのみ生成する。

## 2.2 note

ABI 1.1はConditional Regionを表現しない。Migratorは旧authority-only契約をDynamic Contract schema 6、Conditional Region空集合へ変換する。このため直接生成したSGE4UNI 2.8 authority-only Packageとbyte一致する。Conditional Regionを旧bytesから推測してはならない。


## 2.3 note

ABI 1.1のComposition ContractはBuffer Flowだけを表現する。MigratorはBuffer-only corpusをContract Data schema 3／Verified Decision Data schema 3へCanonical再符号化し、直接生成したSGE4UNI 2.8 Buffer Compositionとbyte一致させる。

Texture2D shapeを旧bytes、endpoint名、Leaf shader、formatから推測してはならない。Texture2Dを含むComposition inputをABI 1 migration writerへ渡した場合は明示拒否する。


## 2.4 note

ABI 1.1はIndirect Dispatch routeを表現しない。Migratorは旧authority-only契約をDynamic Contract schema 6、`IndirectExecutionMode=None`へ明示変換し、直接生成したSGE4UNI 2.8 authority-only Packageとbyte一致させる。

対象Compute Leaf、Compute Command、maxWorkCountを旧Leaf operationや命名から推測してはならない。Verified Dispatchは新規Composition入力からだけ生成する。

## Generalization 6 migration rule

ABI 1.1はCanonical member payloadまたはmulti-route tableを表現しない。Migratorはauthority-only契約だけを`canonicalMemberBytes = 0`、route table空のDynamic Contract schema 6へ変換する。Verified routeを旧bytesから推測しない。直接生成したauthority-only SGE4UNI 2.8とbyte一致しなければならない。


## Generalization 7 Temporal境界

ABI 1.1はResource lifetime、history depth、Previous／Current世代、Temporal Planを表現しない。Migration corpusはsame-frame Buffer Flowだけを受理し、Temporal Buffer入力を明示拒否する。旧bytesからTemporal lifetimeやseed policyを推測してはならない。


## Generalization 8 Compact Worklist境界

ABI 1.1はCompact Worklist mode、index-list Dynamic Slot、Canonical member ID列を表現しない。Migratorはauthority-only契約を`CompactWorklistMode=None`、target Slot無効のDynamic Contract schema 6へ変換する。LeafのDynamic Slot形状やShader命名からworklist routeを推測してはならない。
