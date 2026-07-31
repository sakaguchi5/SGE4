# v1.5.1 バッチ解析互換性修正

> Historical note: この修正報告はv1.5.xのABI 1.x Baselineに対する記録である。現行Production Frozen CompositionはSGE4UNI 2.1である。
## 症状

PowerShellから `run_new_sge4_full_gate.bat` を起動すると、処理開始直後に次のエラーが発生する環境がありました。

```text
'l' は、内部コマンドまたは外部コマンド、
操作可能なプログラムまたはバッチ ファイルとして認識されていません。
```

## 原因

v1.5では、一部のバッチファイルがLFのみの改行で保存され、同じファイルにUTF-8日本語文字列も含まれていました。`cmd.exe`はバッチファイルのUTF-8／改行処理が環境依存であり、CP932環境や日本語を含むパスではコマンド境界を誤解析する可能性があります。

## 修正

- 全 `.bat` をASCII文字のみへ変更
- 全 `.bat` をCRLFへ統一
- 日本語表示を `tools/write_message.ps1` へ分離
- PowerShellスクリプトをUTF-8 BOM＋CRLFへ統一
- `/m /nr:false` を維持
- C++、Frozen ABI、Verifier、Runtime、D3D12実装、テスト内容は変更しない

## 影響

この変更は起動スクリプトの互換性修正だけです。次の形式と実行意味は変更していません。

- Schema 17 Frozen Leaf Package
- SGE4CMP 1.0
- SGE4UNI 1.1
- SGE4INV 1.1
