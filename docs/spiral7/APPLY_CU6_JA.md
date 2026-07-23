# Spiral 7 CU6 適用手順

基点コミット:

```text
c9f0b5a62e2a7f3c9e0355cdaa1c683819c6dcfa
```

## 1. ZIPをSGE4ルートへ上書き展開

CU6 Overlayには新規・変更ファイルのみが含まれる。削除対象はない。

## 2. Project登録とSource Manifest更新

```powershell
.\run_sge4_5_spiral7_cu6_prepare.bat
```

## 3. 実機測定

```powershell
.\run_sge4_5_spiral7_cu6_measurement_decision_evidence.bat
```

Defaultは高性能順のHardware Adapter index 0を使用する。WARPやSoftware AdapterはCU6測定対象として拒否される。

Adapterや測定量を変更する場合はPowerShell runnerを直接実行する。

```powershell
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass `
  -File .\tests\Run-Spiral7CU6.ps1 `
  -AdapterIndex 0 -Runs 2 -Cycles 1 -Iterations 128 -Warmup 1024 -Attempts 3
```

## 4. 成功表示

```text
SGE4-5 SPIRAL 7 COMPLETION UNIT 6 PASSED
SGE4-5 SPIRAL 7 EXPERIMENT COMPLETE
SPIRAL 7 CLOSURE: OWNER REQUIRED
```

CU6成功はSpiral 7の実験完了を意味するが、OwnerによるClosureを自動実行しない。

## 5. 出力

```text
build/tests/spiral7-cu6/measurement_evidence_v2.bin
build/tests/spiral7-cu6/decision_cases_v2.csv
build/tests/spiral7-cu6/decision_report_v2.txt
```

`decision_report_v2.txt`の`[RANKING]`、`[CROSSOVER-T]`、`[CROSSOVER-A]`をOwner判断の入力として使用する。

## Timestamp Resolution FIX02

`T=0`のB/Cは正しく0 group dispatchになる。0 tick intervalは異常ではなくTimestamp分解能未満であるため、Evidence `S7M2`ではone-tick conservative upper boundとして明示的にcensorする。非ゼロDispatchの0 tickは反復数を自動拡大して再測定する。
