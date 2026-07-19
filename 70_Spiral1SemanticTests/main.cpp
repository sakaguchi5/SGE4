#include "../00_Foundation/Base.h"
#include "../60_PgaRigidTransformSemantic/PgaRigidTransformSemantic.h"
#include "../61_Spiral1Contracts/Spiral1Contracts.h"
#include "../62_Spiral1Corpus/Spiral1Corpus.h"
#include "../67_Spiral1Observer/Spiral1Observer.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <limits>
#include <span>
#include <string>
#include <vector>

namespace
{
namespace sem = sge4_5::spiral1::semantic;
namespace con = sge4_5::spiral1::contracts;
namespace cor = sge4_5::spiral1::corpus;
namespace obs = sge4_5::spiral1::observer;
namespace base = sge4_5::base;

[[nodiscard]] bool Near(float a, float b, float tolerance = 2.0e-5f) noexcept
{
    return std::abs(static_cast<double>(a) - static_cast<double>(b)) <= tolerance;
}

int RunStage04()
{
    if (sizeof(sem::Float4Point) != 16) return 1;
    auto identity = sem::BuildIdentityMotor();
    const auto identityBytes = identity.Serialize();
    if (identityBytes.size() != 72 || identity.Qr()[0] != 1.0 || identity.Qd() != std::array<double,4>{}) return 2;
    auto identityRoundTrip = sem::CanonicalPgaMotorV1::Deserialize(identityBytes);
    if (!identityRoundTrip || identityRoundTrip.Value().Serialize() != identityBytes) return 3;

    auto positive = sem::UnitQuaternion::FromComponents(0.5, -0.25, 0.75, 0.125);
    auto negative = sem::UnitQuaternion::FromComponents(-0.5, 0.25, -0.75, -0.125);
    if (!positive || !negative || positive.Value().Components() != negative.Value().Components()) return 4;
    auto motorA = sem::BuildMotor(positive.Value(), {3.25,-2.5,7.75});
    auto motorB = sem::BuildMotor(negative.Value(), {3.25,-2.5,7.75});
    if (!motorA || !motorB || motorA.Value().Serialize() != motorB.Value().Serialize()) return 5;

    auto identityRotation = sem::UnitQuaternion::FromComponents(1,0,0,0);
    auto translated = sem::BuildMotor(identityRotation.Value(), {2,-4,6});
    if (!translated || translated.Value().Qd() != std::array<double,4>{0,1,-2,3}) return 6;

    auto zero = sem::UnitQuaternion::FromComponents(0,0,0,0);
    if (zero) return 7;
    auto nonFinite = sem::UnitQuaternion::FromComponents(std::numeric_limits<double>::infinity(),0,0,0);
    if (nonFinite) return 8;

    auto corrupt = identityBytes;
    corrupt[0] = std::byte{2};
    if (sem::CanonicalPgaMotorV1::Deserialize(corrupt)) return 9;
    corrupt = identityBytes;
    corrupt[4] = std::byte{1};
    if (sem::CanonicalPgaMotorV1::Deserialize(corrupt)) return 10;
    corrupt = motorA.Value().Serialize();
    for (std::size_t offset = 8; offset < corrupt.size(); offset += 8) corrupt[offset + 7] ^= std::byte{0x80};
    if (sem::CanonicalPgaMotorV1::Deserialize(corrupt)) return 11;
    corrupt = identityBytes;
    corrupt[47] = std::byte{0x80}; // qd[0] negative zero sign byte
    if (sem::CanonicalPgaMotorV1::Deserialize(corrupt)) return 12;

    sem::Float4Point valid{1,2,3,1};
    if (!sem::ValidatePoint(valid)) return 13;
    auto invalidW = valid; invalidW.w = 0;
    if (sem::ValidatePoint(invalidW)) return 14;
    auto invalidRange = valid; invalidRange.x = 10001;
    if (sem::ValidatePoint(invalidRange)) return 15;

    const auto corpusIdentity = base::Sha256(std::as_bytes(std::span("stage04", 7)));
    auto apply = sem::BuildApplyPgaMotorSemanticV1(identity, 64, corpusIdentity, con::ObservationContractIdentityV1());
    if (!apply) return 16;
    const auto semanticBytesA = sem::SerializeApplyPgaMotorSemanticV1(apply.Value());
    const auto semanticBytesB = sem::SerializeApplyPgaMotorSemanticV1(apply.Value());
    if (semanticBytesA != semanticBytesB || sem::ApplyPgaMotorSemanticIdentityV1(apply.Value()) != base::Sha256(semanticBytesA)) return 17;
    if (sem::BuildApplyPgaMotorSemanticV1(identity,0,corpusIdentity,con::ObservationContractIdentityV1())) return 18;
    if (sem::BuildApplyPgaMotorSemanticV1(identity,64,corpusIdentity,{})) return 19;

    std::cout << "Stage 04 PGA canonical semantic tests passed.\n";
    return 0;
}

int RunStage05()
{
    auto corpusA = cor::BuildQualificationCorpusV1();
    auto corpusB = cor::BuildQualificationCorpusV1();
    if (!corpusA || !corpusB) { std::cerr << (corpusA ? corpusB.Error() : corpusA.Error()) << '\n'; return 30; }
    if (corpusA.Value().cases.size() != 31 || cor::TotalPointCount(corpusA.Value()) != 25921) return 31;
    std::array<std::uint32_t,16> scenarioCounts{};
    for (const auto& value : corpusA.Value().cases)
    {
        if (value.scenarioId.size() != 3 || value.scenarioId[0] != 'S') return 32;
        const std::uint32_t index = static_cast<std::uint32_t>((value.scenarioId[1]-'0')*10 + (value.scenarioId[2]-'0'));
        if (index > 15) return 33;
        ++scenarioCounts[index];
        if (value.inputPoints.size() != value.referencePoints.size() || value.caseIdentity == base::Digest256{}) return 34;
    }
    for (std::uint32_t i=0;i<15;++i) if (scenarioCounts[i] != 1) return 35;
    if (scenarioCounts[15] != 16) return 36;

    const auto& s02 = corpusA.Value().cases[2];
    if (!Near(s02.referencePoints[0].x,0.25f) || !Near(s02.referencePoints[0].y,-5.5f) || !Near(s02.referencePoints[0].z,4.75f)) return 37;
    const auto& s03 = corpusA.Value().cases[3];
    if (!Near(s03.referencePoints[0].x,-3.0f) || !Near(s03.referencePoints[0].y,3.0f) || !Near(s03.referencePoints[0].z,-3.0f)) return 38;

    const auto bundleA = cor::SerializeCorpusFoundationBundleV1(corpusA.Value());
    const auto bundleB = cor::SerializeCorpusFoundationBundleV1(corpusB.Value());
    if (bundleA != bundleB || base::Sha256(bundleA) != base::Sha256(bundleB)) return 39;

    if (con::UlpDistance(0.0f,-0.0f) != 0) return 40;
    if (con::UlpDistance(1.0f,std::nextafter(1.0f,2.0f)) != 1) return 41;
    auto perfect = obs::ObserveCaseV1(s02,s02.referencePoints,s02.referencePoints);
    if (!perfect || perfect.Value().report.mismatchCount != 0 || perfect.Value().report.nonFiniteCount != 0) return 42;
    const auto perfectBytes = con::SerializeComparisonRecordsV1(perfect.Value().records);
    if (perfectBytes.size() != s02.referencePoints.size() * con::ComparisonRecordBytesV1) return 43;

    auto changed = s02.referencePoints;
    changed[0].x += 0.01f;
    auto detected = obs::ObserveCaseV1(s02,changed,s02.referencePoints);
    if (!detected || detected.Value().report.mismatchCount == 0 || detected.Value().report.firstMismatchIndex != 0 ||
        (detected.Value().records[0].mismatchFlags & (1u << 8)) == 0) return 44;

    auto observationBundleA = obs::BuildObservationFoundationBundleV1(corpusA.Value());
    auto observationBundleB = obs::BuildObservationFoundationBundleV1(corpusB.Value());
    if (!observationBundleA || !observationBundleB || observationBundleA.Value() != observationBundleB.Value()) return 45;
    if (con::SerializeObservationContractV1(con::CanonicalObservationContractV1()).empty() || con::ObservationContractIdentityV1() == base::Digest256{}) return 46;

    std::cout << "Stage 05 corpus, CPU reference, comparison record, and deterministic observer tests passed.\n";
    return 0;
}

std::vector<std::byte> BuildStage04Bundle()
{
    base::BinaryWriter writer;
    constexpr std::string_view magic = "SGE4-5.Stage04.SemanticBundle.V1";
    writer.WriteU32(static_cast<std::uint32_t>(magic.size()));
    writer.WriteBytes(std::as_bytes(std::span(magic.data(),magic.size())));
    auto q1 = sem::UnitQuaternion::FromAxisAngle({1,0,0},0.375);
    auto q2 = sem::UnitQuaternion::FromAxisAngle({2,-1,3},0.7);
    std::array<sem::CanonicalPgaMotorV1,3> motors{
        sem::BuildIdentityMotor(),
        sem::BuildMotor(q1.Value(),{1,2,3}).Value(),
        sem::BuildMotor(q2.Value(),{4.5,-3.25,1.75}).Value()
    };
    const auto corpusId = base::Sha256(std::as_bytes(std::span("stage04-bundle",14)));
    for (std::uint32_t i=0;i<motors.size();++i)
    {
        const auto motorBytes = motors[i].Serialize();
        writer.WriteBytes(motorBytes);
        auto apply = sem::BuildApplyPgaMotorSemanticV1(motors[i],64+i,corpusId,con::ObservationContractIdentityV1());
        const auto semanticBytes = sem::SerializeApplyPgaMotorSemanticV1(apply.Value());
        writer.WriteU32(static_cast<std::uint32_t>(semanticBytes.size()));
        writer.WriteBytes(semanticBytes);
    }
    return std::move(writer).Take();
}

int EmitBundle(const std::string& mode, const std::filesystem::path& path)
{
    std::vector<std::byte> bytes;
    if (mode == "--emit-stage04") bytes = BuildStage04Bundle();
    else
    {
        auto corpusValue = cor::BuildQualificationCorpusV1();
        if (!corpusValue) { std::cerr << corpusValue.Error() << '\n'; return 70; }
        if (mode == "--emit-stage05") bytes = cor::SerializeCorpusFoundationBundleV1(corpusValue.Value());
        else
        {
            auto result = obs::BuildObservationFoundationBundleV1(corpusValue.Value());
            if (!result) { std::cerr << result.Error() << '\n'; return 71; }
            bytes = std::move(result.Value());
        }
    }
    if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
    auto written = base::WriteAllBytes(path,bytes);
    if (!written) { std::cerr << written.Error() << '\n'; return 72; }
    std::cout << path.string() << " bytes=" << bytes.size() << " sha256=" << base::ToHex(base::Sha256(bytes)) << '\n';
    return 0;
}
}

int main(int argc, char** argv)
{
    if (argc == 3 && (std::string(argv[1]) == "--emit-stage04" || std::string(argv[1]) == "--emit-stage05" || std::string(argv[1]) == "--emit-cu1"))
        return EmitBundle(argv[1],argv[2]);
    if (argc == 2 && std::string(argv[1]) == "--stage04") return RunStage04();
    if (argc == 2 && std::string(argv[1]) == "--stage05")
    {
        const int first = RunStage04();
        return first == 0 ? RunStage05() : first;
    }
    const int first = RunStage04();
    if (first != 0) return first;
    const int second = RunStage05();
    if (second != 0) return second;
    std::cout << "SGE4-5 Spiral 1 Completion Unit 1 tests passed.\n";
    return 0;
}
