# SGE4 テストフォルダ

このフォルダは、SGE4のテストを「毎回全部実行する」方式から、用途と変更範囲に応じて選択する方式へ分離します。

完全Qualificationは削除していません。日常開発では軽いSuiteを使い、Stage完成時だけ正式Freezeを実行します。

## 最初に使う4コマンド

```bat
rem 日常開発。GPU実行なし。最も頻繁に使う。
tests\run_dev.bat

rem コミット前。Debug全体ビルドと主要な非GPU統合テスト。
tests\run_gate.bat

rem Stage内の節目。Debugの全回帰、WARP、54 Package corpusを含む。
tests\run_regression.bat

rem Stage完成判定。Debug/Release、fresh process、Recoveryを含む完全Qualification。
tests\run_freeze.bat
```

通常はDebugです。個別SuiteのRelease確認は、次のように構成名を渡します。

```bat
tests\run_planning.bat Release
```

Freezeは内部でDebugとReleaseの両方を実行するため、構成指定は不要です。

すべてのBAT Runnerは実行前にPowerShell構文、UTF-8 BOM、CRLFを検査します。これにより、`${name}:` が必要な箇所の構文事故や文字コードの再発を早期に拒否します。

## 変更領域別のコマンド

```bat
tests\run_architecture.bat
tests\run_semantic.bat
tests\run_planning.bat
tests\run_authority.bat
tests\run_package.bat
tests\run_runtime.bat
tests\run_frontend.bat
tests\run_corpus.bat
tests\run_adversarial.bat
```

詳しい運用、各Suiteの内容、変更領域との対応は `OPERATIONS_GUIDE_JA.md` を参照してください。

## ログ

全Suiteは次へログを保存します。

```text
docs/test-logs/YYYYMMDD-HHMMSS-Suite-Configuration.log
```

このログはQualificationの証拠としてGitへコミットします。`build`成果物とは分離され、SOURCE_MANIFESTのソース固定対象から生成`.log`だけが除外されます。

今回のRunner全体見直し内容は `SCRIPT_REVIEW_JA.md` に記録しています。

## Spiral 2の最終確認

次能力を選ぶ前のSpiral 2正式Qualificationは、リポジトリ直下から次を実行します。

```bat
run_sge4_5_spiral2_final_qualification.bat
```

実GPU測定を再取得する場合だけ`-IncludeRealGpu`を付けます。

