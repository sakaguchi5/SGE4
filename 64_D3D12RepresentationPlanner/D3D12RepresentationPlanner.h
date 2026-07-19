#pragma once

#include "../60_PgaRigidTransformSemantic/PgaRigidTransformSemantic.h"
#include "../63_D3D12RepresentationCandidate/D3D12RepresentationCandidate.h"

#include <array>
#include <string>

namespace sge4_5::spiral1::representation
{
[[nodiscard]] base::Result<RawRepresentationCandidateV1, std::string> PlanRepresentationCandidateV1(
    const semantic::ApplyPgaMotorSemanticV1& semanticValue,
    const RepresentationTargetProfileV1& targetProfile,
    LoweringKindV1 loweringKind);
[[nodiscard]] base::Result<std::array<RawRepresentationCandidateV1, 2>, std::string> PlanAllRepresentationCandidatesV1(
    const semantic::ApplyPgaMotorSemanticV1& semanticValue,
    const RepresentationTargetProfileV1& targetProfile);
}
