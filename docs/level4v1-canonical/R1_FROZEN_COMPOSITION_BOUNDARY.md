# SGE4 Level 4 v1 Canonical Reconstruction
## R1 — Frozen Composition Boundary

## 1. R1の目的

R1はComposition Compiler、Planner、Verifier、Runtimeを作る段階ではない。
最終的にそれらが受け渡す唯一の固定成果物であるFrozen Compositionの国境を、上流実装より先に成立させる。

R1で証明する命題は次である。

> 二つ以上の検証済みD3D12 Schema 17 / Runtime 17 Frozen Leaf Packageと、
> Composition Contract bytes、Verified Decision bytes、Verification Certificate bytesを、
> Source、Compiler、Planner、Runtime、Backendに依存しない、固定幅・決定的・自己完結・破損検出可能な
> Frozen Compositionへ固定し、別プロセスで厳格に復号できる。

R1はContractの意味、Planの正しさ、Certificateの真正性をまだ証明しない。
それらはR2で具体化する。R1では、その三者を後からRuntimeが推測せずに受け渡せる不変Sectionとして固定する。

## 2. 依存境界

`16_FrozenCompositionArtifact`が直接参照してよいProjectは次だけである。

- `00_Foundation`
- `09_FrozenPackageCore`

次を参照してはならない。

- Semantic Model / Analysis
- Execution Plan / Candidate Planner
- SGE4 Compiler
- Package Runtime
- D3D12 Backend
- Frontend
- Windows / D3D12 header

Artifact ReaderがEmbedded Leafへ行う唯一の外部検証は、既存`PackageReader`によるFrozen Package container検証と、
D3D12 Schema 17 / Runtime 17 header契約の照合である。

## 3. Binary container

拡張子はR1資格試験では`.sgec`を使用する。

- Magic: `SGE4CMP\0`
- Container format: 1.0
- Header: 192 bytes
- Section descriptor: 80 bytes
- Endian: little endian
- Digest: SHA-256
- Section alignment: 8 bytes
- Compression: 禁止
- Timestamp、pointer、process固有値、random UUID: 禁止

Headerは次のdigestを分離して保持する。

- Semantic digest
  - Manifest
  - Leaf Table
  - Leaf Bytes
  - Contract Data
- Decision digest
  - Verified Decision Data
- Certificate digest
  - Verification Certificate
- File digest
  - file全体。計算時のみFile digest fieldを0にする

## 4. Canonical sections

R1 containerは次の六Sectionを、この順番で一つずつ必ず持つ。

1. Manifest
2. Leaf Table
3. Leaf Bytes
4. Contract Data
5. Verified Decision Data
6. Verification Certificate

未知Section、任意Section、Section順序変更、末尾dataをR1では許可しない。
将来の拡張を予測したSection reservationも行わない。

## 5. Embedded Leaf contract

R1のCompositionは二つ以上のLeafを必要とする。
各Leafは次を満たす。

- 有効なFrozen Executable Package bytes
- Target kind: D3D12
- Target schema: 17
- Minimum runtime: 17
- 非0の32-byte stable key
- stable keyがComposition内で一意

Writerはauthor declaration順を保存しない。
stable keyのbyte辞書順でLeafを並べ、0から連続するcanonical Leaf IDを割り当てる。
Presenterもauthor提供ordinalではなくstable keyで指定し、Writerがcanonical Leaf IDへ解決する。

Leaf Table recordは128 bytes固定である。

- Leaf ID
- flags
- stable key
- Leaf Bytes Section相対offset
- byte size
- embedded Package execution digest
- embedded Package file digest
- reserved

Leaf bytesはcanonical Leaf ID順、8-byte alignment、zero paddingで格納する。
末尾の余分なpaddingは許可しない。

## 6. Reader validation order

ReaderはD3D12 objectを一つも作らず、概念的に次を検証する。

1. 最小file size
2. Magic
3. Container versionと固定record size
4. Endian、digest algorithm、flags、reserved
5. file sizeとFile digest
6. Section count、固定順序、固定flags、alignment
7. Section offset、size、canonical layout、末尾data禁止
8. Section digest
9. Manifest fixed record
10. Leaf countとPresenter ID
11. Leaf Table stride、連続ID、一意で昇順のstable key
12. Leaf Bytes canonical offset
13. Embedded Frozen Package container
14. Embedded D3D12 Schema 17 / Runtime 17
15. Leaf Table digestとEmbedded Package digestの一致
16. Contract / Decision / Certificate length
17. Semantic / Decision / Certificate digest

すべて成功してから、immutableな`FrozenComposition`を返す。

## 7. R1 Negative Gates

R1資格試験は少なくとも次を拒否する。

- Leafが一つだけ
- duplicate stable key
- empty certificate
- Schema 18 Leaf
- bad magic
- raw byte corruption
- Section順序改変
- 範囲外Presenter
- non-canonical Leaf offset
- non-zero section alignment padding
- outer digestを修復したEmbedded Leaf corruption
- Section digestとFile digestだけを修復したContract mutation

最後の二つにより、外側のhashだけを再計算しても内側のPackage authorityやSemantic digestを偽造できないことを確認する。

## 8. Determinism

同じ意味入力に対して次を要求する。

- Leaf author declaration順を反転しても同じbytes
- Readして同じBuild Inputへ戻し、再Writeして同じbytes
- Fresh Debug process AとBで同じbytes
- DebugとReleaseで同じbytes

旧F11のFrozen Composition bytesとの一致は要求しない。

## 9. R1で証明しないこと

- Composition Endpointの導出
- Resource Flow Contractの正しさ
- Plannerのallocation / schedule / binding
- Independent Verifier
- Certificateの生成または真正性
- Runtime execution
- Shared Buffer
- completion token
- DeviceDomain / epoch
- Recovery
- WARP Composition execution

R1のContract、Decision、Certificate Sectionは非空かつdigest保護されたopaque bytesである。
Runtimeがそれらを実行できるという主張はR2以降まで行わない。

## 10. R1完成条件

次をすべて通過した時だけR1完成候補とする。

- R1 dependency boundary
- Writer / Reader positive tests
- 全Negative Gate
- canonical round trip
- Fresh Debug process identity
- Debug / Release identity
- Stage 0D Foundation Freeze回帰

実際のWindows / MSVC実行結果が通るまでは完成宣言を行わない。
