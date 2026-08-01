# Frozen Artifact Hierarchy Contract — ABI 2.0 Revision

この文書名は既存参照を保つためV1を維持するが、内容はFrozen Composition ABI 2.0の正本構造を表す。

## Frozen Leaf

既存Schema 17の完全な実行成果物を維持する。Shader binary、Resource、Allocation、View、Binding、Executable、Operation stream、Invocation schemaを保持する。

`LeafCertificate`は完全PackageとD3D12 Package Viewから直接生成され、次をbindする。

- Semantic identity
- Package execution identity
- Frozen artifact identity
- Target profile identity
- Resource contract identity
- Write set identity
- Operation sequence identity
- Verification seal identity

Certificate生成のために第二Planner／Verifierを実行してはならない。

## Frozen Composition

Production Frozen Compositionは平坦な`SGE4UNI 2.2`である。

```text
SGE4UNI 2.2
  Manifest
  Leaf Table
  complete Schema 17 Leaf Package bytes
  Contract Data
  Verified Decision Data
  Verification Certificate
  Authority Ledger
  Dynamic Contract
```

`SGE4CMP 1.0`を内包する`CompleteComposition` Sectionは存在しない。Leaf Packageだけが独立した下位Artifactとして保持される。

`CompositionCertificate`は、ABI 2.0 Composition Core digestと検証済みContract／Plan／Sealから直接生成され、次をbindする。

- Frozen Composition identity
- Contract identity
- Plan identity
- Seal identity
- Schedule identity
- Recovery set identity
- Leaf／Flow count

ABI 1.1／SGE4CMP 1.0はmigration資格試験に限定し、Production Readerから参照しない。

## Frozen Dynamic Invocation

`SGE4INV` major 1／minor 3は、正確な集合、Transition record、前History identity、次History、Sealとidentityを保存する。RuntimeがCountだけからMembershipを推測すること、および受理済みHistoryとは異なるHistoryから作られたContinueHistory成果物をSubmitすることを禁止する。

生成経路は一つである。

```text
Request -> Dynamic Planner -> Independent Verifier -> Freeze
```

## Digest layers

- embedded Schema 17 Leaf execution／file digest
- Leaf Certificate identities
- ABI 2.0 Composition Core digest
- Composition Certificate identities
- outer SGE4UNI semantic／file digest
- Frozen Dynamic Invocation identity／history identity

Composition Readerは外側Section、Leaf Table、各Leaf Package、Contract、Plan、Verifier Certificate、Authority Ledger、Dynamic Contractを物質化前に検証する。どれか一つでも不一致なら拒否する。
