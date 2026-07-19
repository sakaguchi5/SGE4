#pragma once

#include "../00_Foundation/Base.h"
#include "../60_PgaRigidTransformSemantic/PgaRigidTransformSemantic.h"
#include "../61_Spiral1Contracts/Spiral1Contracts.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace sge4_5::spiral1::corpus
{
struct QualificationCaseV1 final
{
    std::string scenarioId;
    std::uint32_t subscenarioIndex = 0;
    std::uint64_t seed = 0;
    semantic::CanonicalPgaMotorV1 motor;
    std::vector<semantic::Float4Point> inputPoints;
    std::vector<semantic::Float4Point> referencePoints;
    base::Digest256 caseIdentity{};

    QualificationCaseV1(
        std::string id,
        std::uint32_t subIndex,
        std::uint64_t seedValue,
        semantic::CanonicalPgaMotorV1 motorValue,
        std::vector<semantic::Float4Point> input,
        std::vector<semantic::Float4Point> reference,
        base::Digest256 identity)
        : scenarioId(std::move(id)), subscenarioIndex(subIndex), seed(seedValue),
          motor(std::move(motorValue)), inputPoints(std::move(input)),
          referencePoints(std::move(reference)), caseIdentity(identity) {}
};

struct QualificationCorpusV1 final
{
    base::Digest256 contractIdentity{};
    std::vector<QualificationCaseV1> cases;
};

[[nodiscard]] base::Result<semantic::Float4Point, std::string> EvaluateCpuReferenceV1(
    const semantic::CanonicalPgaMotorV1& motor,
    const semantic::Float4Point& point);
[[nodiscard]] base::Result<std::vector<semantic::Float4Point>, std::string> EvaluateCpuReferenceBatchV1(
    const semantic::CanonicalPgaMotorV1& motor,
    std::span<const semantic::Float4Point> points);
[[nodiscard]] base::Digest256 CorpusContractIdentityV1();
[[nodiscard]] base::Result<QualificationCorpusV1, std::string> BuildQualificationCorpusV1();
[[nodiscard]] std::vector<std::byte> SerializeCorpusFoundationBundleV1(
    const QualificationCorpusV1& corpusValue);
[[nodiscard]] std::size_t TotalPointCount(const QualificationCorpusV1& corpusValue) noexcept;
}
