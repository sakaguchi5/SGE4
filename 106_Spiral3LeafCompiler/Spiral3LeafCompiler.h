#pragma once
#include "../00_Foundation/Base.h"
#include "../80_HierarchySemantic/HierarchySemantic.h"
#include "../100_ReuseSweepSemantic/ReuseSweepSemantic.h"
#include "../102_Spiral3Corpus/Spiral3Corpus.h"
#include "../103_ReuseRepresentationCandidate/ReuseRepresentationCandidate.h"
#include "../105_ReuseRepresentationVerifier/ReuseRepresentationVerifier.h"
#include <cstdint>
#include <string>
#include <vector>

namespace sge4_5::spiral3::leaf
{
struct FrozenReuseLeafV1 final
{
    std::string role;
    base::Digest256 reuseSemanticIdentity{};
    base::Digest256 localPointCorpusIdentity{};
    base::Digest256 verifiedRepresentationCertificate{};
    base::Digest256 verifiedPlanIdentity{};
    base::Digest256 verifiedOperationIdentity{};
    base::Digest256 verifiedProgramTemplateIdentity{};
    candidate::CandidateKindV1 candidateKind{};
    candidate::MatrixLoweringPositionV1 loweringPosition{};
    candidate::OperationKindV1 operation{};
    std::uint32_t operationOrdinal=0;
    std::uint32_t reuseCount=0;
    std::uint32_t pointCount=0;
    base::Digest256 executionDigest{};
    base::Digest256 fileDigest{};
    base::Digest256 packageBindingIdentity{};
    base::Digest256 packageProgramInterfaceDigest{};
    base::Digest256 packageBindingLayoutDigest{};
    base::Digest256 packageShaderBytecodeDigest{};
    std::uint32_t plannedDispatchX=0,plannedDispatchY=1,plannedDispatchZ=1;
    std::uint32_t primaryInputStride=0,primaryInputElementCount=0;
    std::uint32_t primaryOutputStride=0,primaryOutputElementCount=0;
    std::uint32_t secondaryInputStride=0,secondaryInputElementCount=0;
    std::uint32_t expectedDynamicSlotCount=0,expectedExternalSlotCount=0;
    std::uint32_t lockedDynamicBytes=0,lockedReferenceBytes=0;
    std::uint64_t packageOwnedInitialBytes=0;
    std::vector<std::byte> packageBytes;
};
struct FrozenReuseLeafGroupV1 final
{
    candidate::CandidateKindV1 kind{};
    candidate::MatrixLoweringPositionV1 loweringPosition{};
    base::Digest256 reuseSemanticIdentity{};
    base::Digest256 verifierCertificate{};
    base::Digest256 verifiedPlanIdentity{};
    std::uint32_t reuseCount=0,pointCount=0;
    std::vector<FrozenReuseLeafV1> leaves;
};
struct LeafCompileErrorV1 final{std::string stage;std::string message;};

[[nodiscard]] base::Result<FrozenReuseLeafV1,LeafCompileErrorV1> CompileDynamicInvocationSourceV1(
    const spiral2::hierarchy::RigidHierarchySemanticV1& hierarchy,
    const semantic::ReuseSweepSemanticV1& semantic,
    const verification::VerifiedReuseRepresentationV1& verified);
[[nodiscard]] base::Result<FrozenReuseLeafV1,LeafCompileErrorV1> CompileConsumerPointSourceV1(
    const corpus::ReuseCaseV1& reuseCase,
    const verification::VerifiedReuseRepresentationV1& verified);
[[nodiscard]] base::Result<FrozenReuseLeafGroupV1,LeafCompileErrorV1> CompileVerifiedReuseLeafGroupV1(
    const spiral2::hierarchy::RigidHierarchySemanticV1& hierarchy,
    const semantic::ReuseSweepSemanticV1& semantic,
    const verification::VerifiedReuseRepresentationV1& verified);
[[nodiscard]] base::Result<void,LeafCompileErrorV1> SealFrozenReuseLeafBindingV1(FrozenReuseLeafV1& value);
[[nodiscard]] base::Result<void,LeafCompileErrorV1> ValidateFrozenReuseLeafV1(const FrozenReuseLeafV1& value);
[[nodiscard]] std::vector<std::byte> SerializeFrozenReuseLeafEvidenceV1(const FrozenReuseLeafV1& value);
}
