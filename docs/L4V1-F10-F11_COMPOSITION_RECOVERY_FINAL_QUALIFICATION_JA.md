# L4V1-F10〜F11 Composition Recovery / Final Qualification

## 目的

F9までに成立したFrozen Static DAG Runtimeを、Device lossを含む明示的なDeviceDomain状態機械へ昇格させる。
F10はComposition全体を唯一の回復単位とし、F11はF1〜F9の回帰を含む最終資格試験を行う。F12の最終Freezeは本工程に含めない。

## F10の回復順序

1. Composition-owned Shared Buffer、completion token、Endpoint tokenを破棄する。
2. 全Leaf Package instanceを破棄し、Package queueをretireする。
3. Shared DeviceDomainへControlledRebuildまたはactual RemoveDeviceを要求する。
4. ControlledRebuildがActiveへ戻ればDevice epochを進め、Frozen Compositionだけから全Leafを再生成する。
5. Verified PlanのResource FlowごとにShared Bufferを再生成し、保存済みComposition initial dataを再適用する。
6. Endpoint tokenとstate trackingを初期状態へ戻す。
7. Frozen canonical scheduleでDAGを再実行する。
8. actual RemoveDeviceではremoved adapter LUIDを除外し、代替adapterがなければAwaitingAdapterへ遷移する。
9. RetryAdapterReacquisitionは現在のadapter topologyを再検査するが、removed LUIDの安全除外を解除しない。
10. 異なるeligible adapterを取得できた場合だけActiveへ戻り、同じFrozen Compositionから再物質化する。

Compiler、Planner、Verifier、LinkerをRecoveryから呼ぶ経路はない。

## actual RemoveDevice契約

`ForceRemovalForTest`はremoved adapterを直ちに再選択しない。removed LUIDはDeviceDomainの寿命中、候補から除外される。

`RetryAdapterReacquisition`は「除外を解除して同じdeviceを再利用する命令」ではない。adapter topologyの変化を再検査する操作である。別のeligible adapterが現れれば回復できるが、WARP-only資格環境では候補が同じremoved WARPだけなので、Retry後も`AwaitingAdapter`を維持する。これは失敗ではなく安全側の正式状態である。

`AwaitingAdapter`ではLeaf、Shared Resource、tokenを保持せず、SubmitとReadbackを拒否する。旧Resourceと旧completion tokenは外部`shared_ptr`が残っていてもRuntimeへ再採用されない。

同一プロセスで必ず回復する経路は`ControlledRebuild`が資格化する。actual RemoveDevice後の同一removed adapter再利用は証明対象にしない。新しいプロセスはFrozen Compositionだけから独立に再物質化できる。

## F11資格試験

通常Runnerは以下を順に実行する。

- F10 Debug / Release recovery tests
- Controlled whole-domain rebuildとDevice epoch前進
- Shared Leaf、Resource Flow、token、stateの再物質化
- actual RemoveDevice
- removed-adapter LUID exclusion
- WARP-onlyのsame-topology RetryがAwaitingAdapterを維持
- AwaitingAdapterでのSubmit / Readback拒否
- stale Resource / tokenのepoch拒否
- Controlled recovery後のdiamond DAG
- Controlled recovery後のsingle-presenter DAG
- fresh Debug 2プロセスとReleaseプロセスがFrozen Compositionを独立に再物質化
- evidence manifest byte identity
- F1、F2、F3、F4〜F6、F7〜F9の各Runner
- Architecture regression

開発中の短い反復には `run_sge4_level4v1_f10_f11_quick.bat` を使用できる。QuickはF10/F11本体とdeterminismを実行するが、既存F1〜F9 RunnerとArchitectureを省略する。完成判定には通常Runnerを使用する。

## 完成条件

`run_sge4_level4v1_f10_f11.bat` の最終バナーが出力され、Debug別プロセスとDebug / Releaseのevidence manifestがbyte-identicalであること。F12だけが残工程となる。
