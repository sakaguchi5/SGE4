# Spiral 1 Canonical Corpus V1

- Contract ID: `SGE4-5.Spiral1.Corpus.V1`
- Status: Frozen
- Machine-readable source: `CORPUS_DEFINITION_V1.json`

## 1. 共通規則

- scenario順はS00からS15。
- point順は生成ループのlexicographic順またはPRNG生成順。
- fixed pointはbinary64で構成後binary32 round-to-nearest-ties-to-even。
- random生成はSplitMix64を使用。
- 乱数は時刻、thread、platformへ依存しない。

SplitMix64:

```text
state += 0x9E3779B97F4A7C15
z = state
z = (z xor (z >> 30)) * 0xBF58476D1CE4E5B9
z = (z xor (z >> 27)) * 0x94D049BB133111EB
return z xor (z >> 31)
```

24-bit unit value:

```text
u24 = next_u64 >> 40
u = u24 / 16777216.0
```

`range(a,b) = a + (b-a)*u`をbinary64で評価し、point保存時にbinary32へ丸める。

random unit axisは三成分を`range(-1,1)`で生成し、normSquared < 2^-20なら再生成し、binary64で正規化する。

## 2. Fixed grids

`Grid4`: coordinates `[-3,-1,1,3]`のx-major/y/z lexicographic 4x4x4、64 points。

`Grid8x8x16`: x,yは`i-3.5`、zは`k-7.5`、x-major/y/z lexicographic、1024 points。

## 3. Scenario一覧

- S00 Identity / point `(0,0,0)`
- S01 Identity / Grid8x8x16
- S02 translation `(3.25,-2.5,7.75)` / Grid4
- S03 +90 degree around X / Grid4
- S04 +90 degree around Y / Grid4
- S05 +90 degree around Z / Grid4
- S06 180 degree around normalized `(1,2,3)` / Grid4
- S07 angle `0.7`, axis normalized `(2,-1,3)`, translation `(4.5,-3.25,1.75)` / 1024 deterministic random points in `[-100,100]`
- S08 angle `2^-20`, axis normalized `(1,-2,0.5)`, translation `(0.001,-0.002,0.003)` / 1024 deterministic random points in `[-10,10]`
- S09 angle `pi-2^-18`, axis normalized `(-2,5,1)`, translation `(-7,4,2.5)` / 1024 deterministic random points in `[-100,100]`
- S10 composition `M3 * M2 * M1` / 1024 deterministic random points in `[-50,50]`
- S11 identity / 4096 negative points, each component in `[-1000,-0.125]`
- S12 S07 motor / 4096 points in `[-10000,10000]`
- S13 S08 motor / 4096 points in `[-2^-12,2^-12]`
- S14 one deterministic random motor / 4096 points in `[-250,250]`
- S15 16 deterministic random motors, each with 256 points in `[-500,500]`

S10 components:

```text
M1 axis (1,0,0), angle 0.375, translation (1,2,3)
M2 axis (0,1,1), angle -1.125, translation (-2,0.5,4)
M3 axis (2,-1,1), angle 0.625, translation (0,-3,1.25)
application order: M1 then M2 then M3
```

## 4. Seed allocation

各scenarioはmachine-readable definitionの64-bit seedを独立に持つ。S15はmaster seedからmotor、point blockの順に単一streamを消費する。subscenario indexは0..15。

## 5. Identity

Scenario identityは次から計算する。

```text
Corpus contract identity
scenario ID
Motor construction record
point generator record
point count
seed
ordered point bytes
reference algorithm version
```

## 6. Qualification rule

S00-S15を一つでも省略、並べ替え、seed変更、point order変更した結果をCorpus V1 qualificationと呼んではならない。拡張はCorpus V2またはSupplemental corpusとする。
