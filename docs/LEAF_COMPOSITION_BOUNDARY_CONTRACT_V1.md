# Leaf / Composition Boundary Contract V1

## Compositionから見えるLeaf情報

- complete Leaf package bytesまたはそのcontent identity
- execution／file／target-profile digest
- Leaf Certificate
- target compatibility identity
- external endpoint schema
- dynamic slot schema
- surface presence
- recovery requirement

## Compositionから見えない情報

- Source Semantic Graph
- Leaf Planner内部表現
- Candidate探索履歴
- Shader source
- Leaf内部Work DAG
- Leaf内部Queue／Barrier決定を再計画する情報

## 禁止事項

- Composition PlannerがLeaf operationを並べ替える
- Composition VerifierがLeaf VerifierのSealを発行する
- Composition Certificate生成時に別Planner／Verifier経路を作る
- RuntimeがEndpoint accessやDynamic membershipを推測する
- BackendがFlowやHistoryを再発見する
- Leaf内部ResourceとComposition Flowを同じ権威型で所有する
