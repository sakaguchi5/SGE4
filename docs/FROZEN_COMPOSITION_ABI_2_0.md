# Frozen Composition ABI 2.0 正式仕様

## 1. 目的

Frozen Composition ABI 2.0は、`NewSGE4 v1.5.2`で成立していた次の階層を平坦化する。

```text
SGE4UNI 1.1
  -> CompleteComposition Section
       -> SGE4CMP 1.0
            -> Schema 17 Leaf Package群
            -> Contract
            -> Verified Decision
            -> Verification Certificate
  -> Authority Ledger
  -> Dynamic Contract
```

ABI 2.0では、内側の`SGE4CMP` Containerを廃止し、Compositionの実行事実を`SGE4UNI 2.0`が直接所有する。

```text
SGE4UNI 2.0
  1 Manifest
  2 LeafTable
  3 LeafBytes
  4 ContractData
  5 VerifiedDecisionData
  6 VerificationCertificate
  7 AuthorityLedger
  8 DynamicContract
```

これはCompositionの意味や能力を変更するものではない。Container階層、所有者、Digest階層、Readerの検証順序を整理するABI migrationである。

## 2. 維持する下位契約

次は変更しない。

```text
Frozen Leaf Package
  Target Schema version: 17
  Minimum Runtime version: 17

Frozen Dynamic Invocation
  Magic: SGE4INV\0
  Format: major 1 / minor 1
  Manifest schema: 2
```

ABI 2.0のLeaf Tableが参照するbytesは、Schema 17 Leaf Packageの完全なfile bytesである。Leafを再符号化、部分展開、統合してはならない。

## 3. ProductionとMigrationの境界

ABI 2.0時点のProduction Readerは`SGE4UNI 2.0`だけを受理した。現行Readerは末尾の2.1 amendmentに従う。

```text
ReadFrozenCompositionPackage
  accepted: SGE4UNI 2.0
  rejected: SGE4UNI 1.1
  rejected: SGE4CMP 1.0
```

ABI 1.1／SGE4CMP 1.0 Reader／Writerは、次のmigration専用treeへ隔離する。

```text
src/composition/migration/abi1/
  FrozenCompositionAbi1Migration.*
  container/
    FrozenCompositionReader.*
    FrozenCompositionWriter.*
    ...
```

Production Runtime、Production Verified Composition Reader、D3D12 Executorは、このlegacy containerへ依存しない。

## 4. Container header

ABI 2.0はCanonical `SectionedArtifact` containerを使用する。

```text
Magic                 "SGE4UNI\0"
Format major          2
Format minor          0
Header bytes          128
Section descriptor    64 bytes
Section count         8
Endian                little endian
Alignment             Sectionごとに8
```

Header layout:

| Offset | Bytes | Field |
|---:|---:|---|
| 0 | 8 | Magic |
| 8 | 2 | Format major |
| 10 | 2 | Format minor |
| 12 | 4 | Header bytes |
| 16 | 4 | Section descriptor bytes |
| 20 | 4 | Section count |
| 24 | 4 | Flags = 0 |
| 28 | 4 | Reserved = 0 |
| 32 | 8 | File bytes |
| 40 | 8 | Section table offset = 128 |
| 48 | 32 | Semantic digest |
| 80 | 32 | File digest |
| 112 | 16 | Reserved = 0 |

Section descriptor layout:

| Offset | Bytes | Field |
|---:|---:|---|
| 0 | 4 | Section kind |
| 4 | 2 | Section schema version |
| 6 | 2 | Flags |
| 8 | 4 | Alignment |
| 12 | 4 | Reserved = 0 |
| 16 | 8 | File offset |
| 24 | 8 | Stored bytes |
| 32 | 32 | SHA-256 section digest |

全Sectionは`Required | ExecutionAffecting`であり、kind昇順、重複なし、alignment 8、padding 0でなければならない。

## 5. Section一覧

| Kind | Name | Schema | 内容 |
|---:|---|---:|---|
| 1 | Manifest | 2 | 件数、byte数、Core／Artifact／Dynamic identity |
| 2 | LeafTable | 1 | 埋め込みLeafのdense record |
| 3 | LeafBytes | 1 | 完全なSchema 17 Leaf Package bytes |
| 4 | ContractData | 1 | Canonical Package Composition Contract |
| 5 | VerifiedDecisionData | 1 | Raw Composition PlanのCanonical encoding |
| 6 | VerificationCertificate | 1 | 独立Verifier Certificate |
| 7 | AuthorityLedger | 2 | CoreとComposition Certificateの全identity |
| 8 | DynamicContract | 1 | UniverseとComposition／Dynamic identity binding |

未知Section、欠落Section、Schema不一致、Flags不一致、Alignment不一致、順序違反はすべて拒否する。

## 6. Manifest schema 2

Manifestは152 bytes固定である。

| Offset | Bytes | Field |
|---:|---:|---|
| 0 | 4 | schemaVersion = 2 |
| 4 | 4 | dynamicUniverseCount |
| 8 | 4 | leafCount |
| 12 | 4 | flowCount |
| 16 | 4 | presenterLeafId または InvalidIndex |
| 20 | 4 | flags = 0 |
| 24 | 8 | LeafBytes section bytes |
| 32 | 8 | ContractData bytes |
| 40 | 8 | VerifiedDecisionData bytes |
| 48 | 8 | VerificationCertificate bytes |
| 56 | 32 | compositionCoreDigest |
| 88 | 32 | compositionArtifactIdentity |
| 120 | 32 | dynamicSemanticIdentity |

条件:

- `dynamicUniverseCount > 0`
- `leafCount >= 2`
- `flowCount > 0`
- PresenterはInvalidまたはLeaf範囲内
- 四つのbyte数は0ではない
- 三つのDigest／identityはzeroではない
- Manifestの件数とPresenterはDecoded Contractと一致する

## 7. Leaf Table schema 1

各recordは128 bytes固定であり、Leaf ID順かつstable key昇順で並べる。

| Offset | Bytes | Field |
|---:|---:|---|
| 0 | 4 | leafId = record index |
| 4 | 4 | flags = 0 |
| 8 | 32 | stableKey |
| 40 | 8 | LeafBytes相対offset |
| 48 | 8 | byteSize |
| 56 | 32 | Schema 17 execution digest |
| 88 | 32 | Schema 17 file digest |
| 120 | 8 | reserved = 0 |

規則:

- IDは0から連続
- stable keyはzero不可、重複不可、厳密昇順
- 各Leafは8-byte境界へ整列
- Leaf間paddingは0
- 末尾に余分なbytesを持たない
- 埋め込みPackageはD3D12 target、Schema 17、Runtime 17
- Tableのexecution／file digestは埋め込みLeaf Headerと一致
- Package Readerによる完全検証に成功する

## 8. Composition Core digest

Composition CoreはSection 2から6までである。

```text
LeafTable
LeafBytes
ContractData
VerifiedDecisionData
VerificationCertificate
```

Core digestは次のdomain-separated payloadから導出する。

```text
domain        "sge4.composition.abi2.core"
schema        1
payload
  core schema
  leaf count
  flow count
  presenter leaf ID
  core Section count = 5
  各Sectionについて
    kind
    schema
    flags
    alignment
    reserved 0
    byte size
    SHA-256(section bytes)
```

Manifest、Authority Ledger、Dynamic ContractはCore digestへ含めない。これらはCoreとCertificateから独立に再導出し、相互照合する。

## 9. Composition Certificate schema 2

Certificateは次をbindする。

- Contract identity
- Plan identity
- Independent verifier seal identity
- Schedule identity
- Recovery set identity
- Leaf count
- Flow count
- ABI 2.0 Composition Core digest

Frozen Composition artifact identityは、Core digestをidentity seedとしてdomain-separatedに導出する。旧SGE4CMP file digestはABI 2.0 identityの入力ではない。

## 10. Authority Ledger schema 2

Authority Ledgerは次を順番に保存する。

```text
schema = 2
compositionCoreDigest
compositionArtifactIdentity
contractIdentity
planIdentity
sealIdentity
scheduleIdentity
recoverySetIdentity
leafCount
flowCount
```

ReaderはDecoded Contract、再検証済みPlan、独立Verifier Certificate、Core digestから全項目を再導出し、byte一致を要求する。

## 11. Dynamic Contract schema 1

Dynamic Contractは次を保存する。

```text
schema = 1
universeCount
compositionArtifactIdentity
dynamicSemanticIdentity
```

Dynamic semantic identityは、ABI 2.0 Core digest、Composition Certificate、Dynamic ContractからCanonicalに導出する。RuntimeはComposition identityの異なるSGE4INVを受理しない。

## 12. Reader検証順序

Production Readerは、物質化前に少なくとも次を行う。

1. 最小file size
2. Magic
3. major = 2
4. minor = 0
5. Header／descriptor size
6. file size
7. Header reserved／flags
8. file digest
9. Section count = 8
10. Section kind厳密昇順／重複拒否
11. Section alignment／range／overlap
12. Section間zero padding
13. Section digest
14. semantic digest
15. exact Section kind／schema／flags／alignment
16. Manifest schema／件数／byte数
17. Leaf Table dense ID／stable order／range／zero padding
18. 各Schema 17 Leafの完全Reader検証
19. Composition Core digest
20. Contract decodeとLeafとの再検証
21. Plan decode
22. 独立Verifierによる再検証・Seal再導出
23. Verification Certificate一致
24. ManifestとContract／Coreの一致
25. Authority Ledgerの全identity一致
26. Dynamic ContractとDynamic semantic identity一致

この検証途中でD3D12 objectを作成してはならない。

## 13. 決定性

同一のValidated Contract、Verified Plan、Dynamic Contractから生成されるABI 2.0 bytesは、次で完全一致しなければならない。

```text
Debug process A
Debug process B
Release process
```

Timestamp、pointer、random UUID、process固有値、unordered iteration orderを含めない。

## 14. Portable Golden Artifact

初期ABI 2.0 Writerの固定Oracleとして、Portable Schema 17 Leaf二個と三Flowから生成するSGE4UNI 2.0を固定する。

```text
File bytes: 9304
SHA-256: 753c82dfc62c65cf56d09bccf81b9b081b2e9c9a04c81ddce2d0b79f36b77223
```

この値が変化する変更は、単なるSource refactoringではなくABI bytes変更として扱い、versioning判断と仕様更新を要求する。

## 15. 破損拒否Corpus

Architecture Gateは少なくとも次を拒否する。

- 必須Section欠落
- 未知Required Section
- Section schema不一致
- descriptor offset範囲外
- descriptor kind重複
- 非dense Leaf ID
- 非Canonical Leaf offset
- 非zero Leaf padding
- 破損したLeaf Package
- 破損したContract
- 破損したVerified Decision
- 破損したVerification Certificate
- 不一致Core digest
- 不一致Frozen Composition identity
- 不一致Authority Ledger
- 不一致Dynamic identity
- Writerへの重複Section入力

## 16. 現在の能力範囲

ABI 2.0は現在実証済みのComposition能力だけを表す。

- Buffer-only finite static DAG
- single writer
- one or more consumers
- Composition input／internal／output Flow
- optional single presenter
- single DeviceDomain
- whole-Composition Recovery
- Frozen Dynamic Invocationとのidentity binding

Texture Flow、Conditional Region、Variant Set、Streaming、Partial Recovery、Multiple Adapterの空Sectionは先回りして追加しない。

---

## Production amendment: SGE4UNI 2.1

Level 4 Generalization 1により、Production minorは2.1へ進んだ。2.0の平坦Composition Core、Leaf bytes、Contract、Verified Decision、Certificate、Authority Ledgerは維持する。

変更点はDynamic Contract schema 2だけである。

```text
executionMode
  AuthorityOnly
  VerifiedDenseSlot

targetLeaf
targetDynamicSlot
memberBytes
```

VerifiedDenseSlotでは、埋め込みSchema 17 LeafのDynamic Slot requiredBytesが`universeCount * memberBytes`と一致することをWriter／Readerが検証する。詳細は`LEVEL4_GENERALIZATION1_VERIFIED_DYNAMIC_EXECUTION.md`を参照する。

## Production amendment: SGE4UNI 2.2

Level 4 Generalization 2によりProduction minorを2.2へ進め、Dynamic Contractをschema 3へ更新した。schema 3はGeneralization 1のVerifiedDenseSlot routeに加え、非ネスト型Conditional Regionのdense ID、exact-set predicate、True／False Leaf集合をCanonical encodingする。Composition Coreおよび埋込みSchema 17 Leaf bytesは2.1から変更しない。

対応するFrozen Dynamic InvocationはSGE4INV 1.3、Manifest schema 4である。必須Conditional Execution Section schema 1が、独立VerifierにSealされたRegion selection、enabled Leaf集合、Conditional Execution identityを保存する。Runtimeはpredicateを再評価しない。
