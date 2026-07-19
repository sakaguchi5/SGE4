# SGE4 Qualification test logs

このフォルダは、階層化テストRunnerが生成するQualificationログの保存先です。

生成形式:

```text
YYYYMMDD-HHMMSS-Suite-Configuration.log
```

運用規則:

- `Dev`、`Gate`、`Regression`、`Freeze`、領域別Suiteの実行ログはここへ生成する。
- `.log`は通常の生成物として`.gitignore`で除外する。
- 正式証拠として残すログだけを内容確認後に明示的に追加する。

```powershell
git add -f docs/test-logs/YYYYMMDD-HHMMSS-Suite-Configuration.log
```

- 原則として、最新のFinal IntegrationまたはFreeze合格ログ、対応する最終Architectureログ、未解決または設計判断上重要な失敗ログだけを正式証拠とする。
- 中間実行、重複実行、修正済みの単純な環境エラーはコミットしない。
- ログは生成物なので`SOURCE_MANIFEST.sha256`の固定対象には含めない。
- C++ビルド成果物や一時テストPackageは従来どおり`build/`へ出力し、Git管理しない。
- ログを書き換えて合格証拠を作らない。必要な場合は再実行し、新しいタイムスタンプのログを追加する。
