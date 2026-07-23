# Spiral 7 CU6 Decision Evidence

## Evidenceの役割

CU6のDecision Evidenceは「勝者を決める命令」ではなく、完成Architectureに対する実機上の局所的な相対性能地図である。

測定caseの座標は次である。

```text
(Pattern, ActiveCount, TransitionCount,
 UpdateCount, ClearCount, AffectedBlockCount)
```

同じ`ActiveCount`と`TransitionCount`であっても、PatternによりAffected Block数が変わり、Candidate Cの仕事量は変化し得る。したがって、`|T_t|`だけでCandidate Cを説明することは禁止する。

## Per-case ranking

Reportは各caseについて次を出力する。

```text
[RANKING] pattern=... A=... T=... U=... C=... blocks=...
          winnerSet=...
          1=X(ns) 2=Y(ns) 3=Z(ns)
```

`winnerSet`は最小medianからrelative noise floor以内にあるCandidate集合である。単独winnerを強制しない。

`censored=A:x/B:y/C:z`はTimestamp分解能未満だったsample数を表す。censored sampleは、0 nsという断定ではなく1 timestamp tickの保守的上限としてmedianとpaired ratioへ入る。censoringは`expectedDispatchGroupsX=0`の場合だけ許可されるため、非ゼロworkを速すぎるという理由で0扱いすることはない。

## Pairwise decision

A/B/Cの6順列を同一blockで測定し、各順列をpaired observationとする。

```text
A/B
B/C
A/C
```

比率がnoise floorを超え、指定割合以上で同じ側が勝つ場合のみPair winnerとする。そうでなければ`NoiseEquivalent`または`Unresolved`である。

## Crossover surface

### Transition方向

```text
[CROSSOVER-T] pattern=P A=K T=t0->t1 WinnerSet0->WinnerSet1
```

固定Pattern・固定Active数で、Transition数に沿ったwinner set変化を記録する。

### Active方向

```text
[CROSSOVER-A] pattern=P T=D A=a0->a1 WinnerSet0->WinnerSet1
```

固定Pattern・固定Transition数で、Active数に沿ったwinner set変化を記録する。

## Owner decision boundary

どの測定結果であっても、CU6が自動的に次を許可することはない。

```text
RuntimeがA/B/Cを選ぶ
BackendがActive/Transition/Patternを観測して選択する
測定GPUのwinnerを普遍的winnerとしてCanonical化する
```

固定される結論は次である。

```text
MeasurementResultScope = AdapterAndDriverSpecificObservation
RuntimeCandidatePolicyAuthorization = None
UniversalWinnerClaim = Forbidden
Spiral7Closure = OwnerRequired
```
