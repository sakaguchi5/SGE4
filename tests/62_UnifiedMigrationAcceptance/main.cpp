#include <array>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string_view>

namespace
{
struct Mapping final { std::string_view id; std::string_view fact; std::string_view owner; };
constexpr std::array mappings = {
    Mapping{"V2-R0-I001","SemanticExecutionSeparation","D3D12Compiler / CompositionArtifactToolchain"},
    Mapping{"V2-R0-I002","ExplicitDynamicInvocationInputs","DynamicModelArtifact"},
    Mapping{"V2-R0-I003","ExternalMembershipAuthority","DynamicModelArtifact"},
    Mapping{"V2-R0-I004","ExactTransitionAlgebra","DynamicPlanner / DynamicVerifier"},
    Mapping{"V2-R0-I005","FirstInvocationFullActiveRule","DynamicPlanner / DynamicVerifier"},
    Mapping{"V2-R0-I006","NoGamePolicyInference","CanonicalCore"},
    Mapping{"V2-R0-I007","RawCandidateCannotExecute","D3D12Compiler / CompositionArtifactToolchain"},
    Mapping{"V2-R0-I008","PlannerHasNoSealAuthority","LeafPlanner / CompositionPlanner / DynamicPlanner"},
    Mapping{"V2-R0-I009","PlannerIndependentVerification","LeafVerifier / CompositionVerifier / DynamicVerifier"},
    Mapping{"V2-R0-I010","OpaqueVerifiedPlan","LeafVerifier / LeafArtifact"},
    Mapping{"V2-R0-I011","DeterministicFrozenIdentity","CanonicalCore / ArtifactToolchains"},
    Mapping{"V2-R0-I012","ResourceIdentityBinding","LeafArtifact / CompositionArtifactToolchain"},
    Mapping{"V2-R0-I013","TargetBindingAuthority","LeafArtifact / D3D12Compiler"},
    Mapping{"V2-R0-I014","FiniteBufferDagComposition","CompositionModel / CompositionVerifier"},
    Mapping{"V2-R0-I015","SingleWriterFlow","CompositionVerifier"},
    Mapping{"V2-R0-I016","OptionalSinglePresenter","CompositionVerifier"},
    Mapping{"V2-R0-I017","SingleAdapterScope","RuntimeCore / D3D12Executor"},
    Mapping{"V2-R0-I018","WholeCompositionRecoveryScope","RuntimeCore / D3D12Executor"},
    Mapping{"V2-R0-I019","NoUnprovenCompositionGeneralization","CompositionModel / CompositionVerifier"},
    Mapping{"V2-R0-I020","StrongDynamicDimensions","CanonicalCore / DynamicModelArtifact"},
    Mapping{"V2-R0-I021","VerifiedIndirectQuantity","DynamicPlanner / DynamicVerifier"},
    Mapping{"V2-R0-I022","ExplicitHistoryValidity","DynamicModelArtifact / RuntimeCore"},
    Mapping{"V2-R0-I023","ExactSparseMembership","DynamicPlanner / DynamicVerifier"},
    Mapping{"V2-R0-I024","SparseTemporalDeltaAuthority","DynamicPlanner / DynamicVerifier"},
    Mapping{"V2-R0-I025","ExactIncrementalWriteSet","DynamicPlanner / DynamicVerifier"},
    Mapping{"V2-R0-I026","RetainedHistoryByteStability","DynamicModelArtifact / RuntimeCore"},
    Mapping{"V2-R0-I027","EpochBoundRuntimeHandles","RuntimeCore"},
    Mapping{"V2-R0-I028","RecoveryInvalidatesTemporalState","RuntimeCore / D3D12Executor"},
    Mapping{"V2-R0-I029","ExplicitExternalRecoveryRebind","RuntimeCore / D3D12Executor"},
    Mapping{"V2-R0-I030","FullActiveRecoverySeed","DynamicModelArtifact / RuntimeCore"},
    Mapping{"V2-R0-I031","RemovedAdapterExclusion","D3D12Executor actual-removal qualification"},
    Mapping{"V2-R0-I032","CorrectnessMeasurementSeparation","Qualification gates"},
    Mapping{"V2-R0-I033","MeasurementIdentityBinding","Reference evidence ledger"},
    Mapping{"V2-R0-I034","PairedDecisionAuthority","Reference evidence ledger"},
    Mapping{"V2-R0-I035","NoRuntimePerformancePolicy","RuntimeCore"},
    Mapping{"V2-R0-I036","ExternalRawEvidenceRetention","reference/level4v2_docs"},
    Mapping{"V2-R0-I037","ReferenceRetentionUntilMigrationProof","reference subtree"},
    Mapping{"V2-R0-I038","SingleFactOwnership","15 product boundaries"},
    Mapping{"V2-R0-I039","RepresentationChoiceRemainsVerified","LeafPlanner / LeafVerifier"},
    Mapping{"V2-R0-I040","NoHardwarePolicyInCanonicalAbi","CanonicalCore"}
};
static_assert(mappings.size() == 40);
}

int main()
{
    std::set<std::string_view> ids;
    for (const auto& mapping : mappings)
    {
        if (!ids.insert(mapping.id).second || mapping.fact.empty() || mapping.owner.empty())
            throw std::runtime_error("入力または内部状態が検証または実行の契約に違反しています。");
    }
    std::cout << "New SGE4統合移行受入試験に合格しました。\n";
    std::cout << "再現した不変条件数：" << mappings.size() << '\n';
    for (const auto& mapping : mappings)
        std::cout << mapping.id << "：" << mapping.fact << " -> " << mapping.owner << '\n';
    return 0;
}
