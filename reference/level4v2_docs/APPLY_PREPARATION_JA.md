# Level 4 v2前整理パッケージの適用

## 基点

```text
f802c12b162569a869c214da22b80142b3a4a0dd
```

このZIPをSGE4ルートへ上書き展開します。

## 実行

```powershell
.\run_sge4_5_pre_level4v2_prepare.bat
.\run_sge4_5_spiral7_reference_gate.bat
```

1本目は新規・変更ファイルを含めて`SOURCE_MANIFEST.sha256`を再生成します。Gitのadd、commit、checkout、resetは行いません。

2本目はSpiral 7 CU1〜CU6の静的な系譜・境界・Manifest検証と、Level 4 v2引継ぎ検証を実行します。CU5の完全WARP実行とCU6の実GPU再測定は繰り返しません。

## 実測生成物

`spiral7-cu6-2.zip`と展開後の`.bin/.csv/.txt`はGitへ追加しません。次のZIP SHA-256を外部Evidenceとして保管します。

```text
9ee852c2a2574a8cb9c163b6e94213b855317bba74319e6443e9aa98c10c24d9
```

## 削除について

この整理ではtracked fileを削除しません。段階別資料とprepare runnerは既存Verifierの検証対象であり、v2の移行証明ができる前に削除すると参照実装の再現性が落ちるためです。

物理的な削除はLevel 4 v2 RoadmapのR7で、置換先Invariantを一つずつ確定してから行います。
