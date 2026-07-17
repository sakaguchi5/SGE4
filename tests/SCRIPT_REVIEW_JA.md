# SGE4 Stage 0D スクリプト全体見直し記録

## 対象

C++実装、ヘッダー、Visual Studioプロジェクト、ソリューションは変更していません。
対象はPowerShell、BAT、スクリプト運用資料、SOURCE_MANIFESTだけです。

## 発見した問題

1. PowerShellの展開可能文字列で、変数直後にコロンを置いていました。
   - `$code:` と `$lineNumber:` はPowerShellで無効な変数参照になります。
   - `${code}:` と `${lineNumber}:` へ修正しました。

2. 個別 `.vcxproj` ビルド時に `SolutionDir` を渡していませんでした。
   - 各プロジェクトの `OutDir` は `$(SolutionDir)build\bin\...` を使用しています。
   - そのためビルド成功後も、Runnerが期待する場所にEXEが生成されない場合がありました。
   - 個別ビルドへ `/p:SolutionDir=<SGE4 root>\` を追加しました。

3. MSBuild出力とWindows PowerShell 5.1の復号設定が一致していませんでした。
   - PowerShell RunnerをUTF-8へ統一しました。
   - PS1/BATをUTF-8 BOM、CRLFへ統一しました。

4. Authority単独Gateも `.vcxproj` を直接ビルドしており、同じ出力先問題を持っていました。
   - 正式なSolution build経路を使用するように変更しました。

5. Freezeは資料上SOURCE_MANIFESTを検査するとしていましたが、実際のStage 0D BATでは依存検査しか実行していませんでした。
   - Freeze開始時にArchitecture Suite全体を実行するように変更しました。

6. READMEが存在しない `run_sge4_final.bat` を案内していました。
   - 正式な `run_sge4_freeze.bat` へ修正しました。

7. 各Suite BATに同じPowerShell起動処理が重複していました。
   - `tests\run_suite.bat` に共通化しました。
   - 構成名の検証と終了コード伝播も一か所へ固定しました。

## 再発防止

`tests\tools\Verify-ScriptContracts.ps1` を追加しました。

全BAT Runnerは本体実行前に次を検査します。

- すべてのPS1がPowerShell Parserを通過する
- PS1/BAT/CMDがUTF-8 BOMである
- 改行がCRLFである
- ファイル末尾に改行がある
- BAT/CMDが `@echo off` から始まる
- 必須Runnerと検査スクリプトが存在する

Architecture Suiteでも同じ検査を正式項目として実行します。

## 推奨確認順

```bat
run_sge4_dev.bat
run_sge4_gate.bat
run_sge4_regression.bat
run_sge4_freeze.bat
```

最初は `run_sge4_dev.bat` を実行し、次に `run_sge4_gate.bat` を実行してください。

## Qualificationログ保存先の改訂

- `tests/Invoke-SGE4Tests.ps1`のログ出力先を`build\test-logs`から`docs\test-logs`へ変更した。
- ログはGitに残す実行証拠とし、`.gitignore`では明示的に追跡対象とした。
- 実行ごとに変化する`docs/test-logs/*.log`だけはSOURCE_MANIFESTのソース固定対象から除外した。
- `docs/test-logs/README_JA.md`は通常の文書としてSOURCE_MANIFESTに含める。

## Visual Studio個人設定ファイルの除外

- Visual Studioが自動生成する`*.vcxproj.user`などの`*.user`をSOURCE_MANIFEST対象外にした。
- あわせて`*.suo`、`*.VC.db`、`*.VC.opendb`も、GitとSOURCE_MANIFESTの両方で端末固有生成物として扱う。
- これらがローカルに存在してもArchitecture Suiteの「Untracked files」にはならない。
