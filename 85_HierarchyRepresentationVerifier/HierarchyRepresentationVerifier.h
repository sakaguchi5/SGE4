#pragma once
#include "../83_HierarchyRepresentationCandidate/HierarchyRepresentationCandidate.h"
#include <span>

namespace sge4_5::spiral2::verification
{
class VerifiedHierarchyRepresentationV1 final
{
public:
    [[nodiscard]] const candidate::RawCandidateGraphV1& Graph() const noexcept{return graph_;}
    [[nodiscard]] const base::Digest256& CertificateIdentity() const noexcept{return certificateIdentity_;}
private:
    VerifiedHierarchyRepresentationV1(candidate::RawCandidateGraphV1 graph,base::Digest256 certificate):graph_(std::move(graph)),certificateIdentity_(certificate){}
    candidate::RawCandidateGraphV1 graph_;
    base::Digest256 certificateIdentity_{};
    friend base::Result<VerifiedHierarchyRepresentationV1,std::string> VerifyCandidateGraphV1(const hierarchy::RigidHierarchySemanticV1&,const candidate::RawCandidateGraphV1&);
};
[[nodiscard]] base::Result<VerifiedHierarchyRepresentationV1,std::string> VerifyCandidateGraphV1(const hierarchy::RigidHierarchySemanticV1& hierarchy,const candidate::RawCandidateGraphV1& proposal);
[[nodiscard]] std::vector<std::byte> SerializeVerifiedHierarchyRepresentationV1(const VerifiedHierarchyRepresentationV1& value);
}
