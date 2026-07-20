#pragma once
#include "../83_HierarchyRepresentationCandidate/HierarchyRepresentationCandidate.h"
#include <span>

namespace sge4_5::spiral2::verification
{
struct VerifiedHierarchyOperationV1 final
{
    std::uint32_t ordinal=0;
    candidate::OperationKindV1 operation{};
    std::uint32_t primaryInputStride=0;
    std::uint32_t primaryInputElementCount=0;
    std::uint32_t primaryOutputStride=0;
    std::uint32_t primaryOutputElementCount=0;
    std::uint32_t secondaryInputStride=0;
    std::uint32_t secondaryInputElementCount=0;
    std::uint32_t secondaryOutputStride=0;
    std::uint32_t secondaryOutputElementCount=0;
    std::uint32_t dispatchX=0,dispatchY=1,dispatchZ=1;
    base::Digest256 programIdentity{};
    base::Digest256 operationIdentity{};
};
struct VerifiedHierarchyExecutionPlanV1 final
{
    base::Digest256 hierarchySemanticIdentity{};
    base::Digest256 dynamicInvocationSchemaIdentity{};
    base::Digest256 observationContractIdentity{};
    candidate::CandidateKindV1 kind{};
    candidate::MatrixLoweringPositionV1 loweringPosition{};
    candidate::HierarchyExecutionModelV1 hierarchyExecutionModel{};
    std::uint32_t boneCount=0;
    std::uint32_t hierarchyDepthCount=0;
    std::uint32_t candidateLeafDispatchCount=0;
    std::vector<VerifiedHierarchyOperationV1> operations;
    std::vector<candidate::CandidateEdgeV1> edges;
    base::Digest256 planIdentity{};
};
class VerifiedHierarchyRepresentationV1 final
{
public:
    [[nodiscard]] const VerifiedHierarchyExecutionPlanV1& Plan() const noexcept{return plan_;}
    [[nodiscard]] const base::Digest256& CertificateIdentity() const noexcept{return certificateIdentity_;}
private:
    VerifiedHierarchyRepresentationV1(VerifiedHierarchyExecutionPlanV1 plan,base::Digest256 certificate):plan_(std::move(plan)),certificateIdentity_(certificate){}
    VerifiedHierarchyExecutionPlanV1 plan_;
    base::Digest256 certificateIdentity_{};
    friend base::Result<VerifiedHierarchyRepresentationV1,std::string> VerifyCandidateGraphV1(const hierarchy::RigidHierarchySemanticV1&,const candidate::RawCandidateGraphV1&);
};
[[nodiscard]] base::Result<VerifiedHierarchyRepresentationV1,std::string> VerifyCandidateGraphV1(const hierarchy::RigidHierarchySemanticV1& hierarchy,const candidate::RawCandidateGraphV1& proposal);
[[nodiscard]] std::vector<std::byte> SerializeVerifiedHierarchyOperationV1(const VerifiedHierarchyOperationV1& value);
[[nodiscard]] std::vector<std::byte> SerializeVerifiedHierarchyExecutionPlanV1(const VerifiedHierarchyExecutionPlanV1& value);
[[nodiscard]] std::vector<std::byte> SerializeVerifiedHierarchyRepresentationV1(const VerifiedHierarchyRepresentationV1& value);
}
