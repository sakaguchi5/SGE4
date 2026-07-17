# SGE4 Qualification test logs

このフォルダは、階層化テストRunnerが生成するQualificationログの正式な保存先です。

生成形式:

```text
YYYYMMDD-HHMMSS-Suite-Configuration.log
```

運用規則:

- `Dev`、`Gate`、`Regression`、`Freeze`、領域別Suiteの実行証拠をここへ残す。
- 合格ログと、調査上重要な失敗ログをGitへコミットする。
- ログは生成物なので`SOURCE_MANIFEST.sha256`の固定対象には含めない。
- C++ビルド成果物や一時テストPackageは従来どおり`build/`へ出力し、Git管理しない。
- ログを書き換えて合格証拠を作らない。再実行して新しいタイムスタンプのログを残す。
