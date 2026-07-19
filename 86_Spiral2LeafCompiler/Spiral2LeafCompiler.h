#pragma once
#include "../00_Foundation/Base.h"
#include "../80_HierarchySemantic/HierarchySemantic.h"
#include "../85_HierarchyRepresentationVerifier/HierarchyRepresentationVerifier.h"
#include <cstdint>
#include <string>
#include <vector>

namespace sge4_5::spiral2::leaf
{
struct FrozenHierarchyLeafV1 final
{
    std::string role;
    base::Digest256 verifiedRepresentationCertificate{};
    base::Digest256 executionDigest{};
    base::Digest256 fileDigest{};
    std::uint32_t lockedDynamicSlot=base::InvalidIndex;
    std::uint32_t lockedDynamicBytes=0;
    std::uint32_t lockedReferenceSlot=base::InvalidIndex;
    std::uint32_t lockedReferenceBytes=0;
    std::uint32_t inputStride=0;
    std::uint32_t outputStride=0;
    std::vector<std::byte> packageBytes;
};
struct FrozenHierarchyLeafGroupV1 final
{
    candidate::CandidateKindV1 kind{};
    base::Digest256 hierarchySemanticIdentity{};
    base::Digest256 verifierCertificate{};
    std::vector<FrozenHierarchyLeafV1> leaves;
};
struct LeafCompileErrorV1 final{std::string stage;std::string message;};
[[nodiscard]] base::Result<FrozenHierarchyLeafV1,LeafCompileErrorV1> CompileDynamicInvocationSourceV1(const hierarchy::RigidHierarchySemanticV1& hierarchy,const verification::VerifiedHierarchyRepresentationV1& verified);
[[nodiscard]] base::Result<FrozenHierarchyLeafGroupV1,LeafCompileErrorV1> CompileVerifiedHierarchyLeafGroupV1(const hierarchy::RigidHierarchySemanticV1& hierarchy,const verification::VerifiedHierarchyRepresentationV1& verified);
[[nodiscard]] base::Result<void,LeafCompileErrorV1> ValidateFrozenHierarchyLeafV1(const FrozenHierarchyLeafV1& value);
[[nodiscard]] std::vector<std::byte> SerializeFrozenHierarchyLeafEvidenceV1(const FrozenHierarchyLeafV1& value);
}
