#include "../00_Foundation/Base.h"
#include "../80_HierarchySemantic/HierarchySemantic.h"
#include "../82_Spiral2Corpus/Spiral2Corpus.h"
#include "../102_Spiral3Corpus/Spiral3Corpus.h"
#include "../103_ReuseRepresentationCandidate/ReuseRepresentationCandidate.h"
#include "../104_ReuseRepresentationPlanner/ReuseRepresentationPlanner.h"
#include "../105_ReuseRepresentationVerifier/ReuseRepresentationVerifier.h"
#include "../107_Spiral3Scenarios/Spiral3Scenarios.h"
#include <array>
#include <filesystem>
#include <iostream>

namespace b=sge4_5::base;
namespace c=sge4_5::spiral3::corpus;
namespace candidate=sge4_5::spiral3::candidate;
namespace planner=sge4_5::spiral3::planner;
namespace verification=sge4_5::spiral3::verification;
namespace scenario=sge4_5::spiral3::scenarios;

namespace
{
void Blob(b::BinaryWriter& writer,std::span<const std::byte> bytes){writer.WriteU64(bytes.size());writer.WriteBytes(bytes);}
std::vector<std::byte> BuildFreezeBundle()
{
    auto hierarchy=c::BuildCanonicalMeasurementHierarchyV1();if(!hierarchy)throw std::runtime_error(hierarchy.Error());auto cases=c::BuildReuseCorpusV1(hierarchy.Value());if(!cases)throw std::runtime_error(cases.Error());auto corpus=c::BuildCorpusFoundationBundleV1();if(!corpus)throw std::runtime_error(corpus.Error());auto frames=sge4_5::spiral2::corpus::BuildFrameCorpusV1(hierarchy.Value());if(!frames)throw std::runtime_error(frames.Error());
    b::BinaryWriter writer;constexpr std::string_view domain="SGE4-5.Spiral3.CU5.ArchitectureFreeze.V1";writer.WriteU32(static_cast<std::uint32_t>(domain.size()));writer.WriteBytes(std::as_bytes(std::span(domain.data(),domain.size())));Blob(writer,corpus.Value());auto hierarchyBytes=sge4_5::spiral2::hierarchy::SerializeRigidHierarchySemanticV1(hierarchy.Value());Blob(writer,hierarchyBytes);writer.WriteU32(static_cast<std::uint32_t>(cases.Value().size()));const std::array kinds{candidate::CandidateKindV1::MatrixHierarchy,candidate::CandidateKindV1::DirectPgaHierarchy,candidate::CandidateKindV1::HybridHierarchy};
    for(const auto& reuseCase:cases.Value())
    {
        auto semantic=sge4_5::spiral3::semantic::SerializeReuseSweepSemanticV1(reuseCase.semantic),points=c::SerializePointsV1(reuseCase.localPoints);writer.WriteU32(reuseCase.reuseCount);Blob(writer,semantic);Blob(writer,points);
        for(const auto kind:kinds)
        {
            auto raw=planner::PlanCandidateGraphV1(hierarchy.Value(),reuseCase.semantic,kind);if(!raw)throw std::runtime_error(raw.Error());auto rawBytes=candidate::SerializeRawCandidateGraphV1(raw.Value());Blob(writer,rawBytes);auto verified=verification::VerifyCandidateGraphV1(hierarchy.Value(),reuseCase.semantic,raw.Value());if(!verified)throw std::runtime_error(verified.Error());auto verifiedBytes=verification::SerializeVerifiedReuseRepresentationV1(verified.Value());Blob(writer,verifiedBytes);
        }
        auto frozen=scenario::BuildFrozenReuseComparisonScenarioV1(hierarchy.Value(),reuseCase);if(!frozen)throw std::runtime_error(frozen.Error().stage+": "+frozen.Error().message);auto frozenBytes=scenario::SerializeFrozenReuseComparisonScenarioEvidenceV1(frozen.Value());Blob(writer,frozenBytes);writer.WriteU32(static_cast<std::uint32_t>(frames.Value().size()));
        for(const auto& frame:frames.Value())
        {
            auto palette=sge4_5::spiral2::contracts::SerializeDynamicLocalMotorPaletteV1(frame.palette);auto reference=c::BuildIndependentCpuReferenceV1(hierarchy.Value(),frame.palette,reuseCase.localPoints,sge4_5::spiral3::contracts::ReuseCountV1{reuseCase.reuseCount});if(!reference)throw std::runtime_error(reference.Error());auto referenceBytes=c::SerializePointsV1(reference.Value());writer.WriteBytes(b::Sha256(palette));writer.WriteBytes(b::Sha256(referenceBytes));
        }
    }
    return std::move(writer).Take();
}
}
int main(int argc,char**argv)
{
    try{auto bundle=BuildFreezeBundle();if(argc==3&&std::string(argv[1])=="--emit"){auto wrote=b::WriteAllBytes(std::filesystem::path(argv[2]),bundle);if(!wrote)return 1;}else if(argc!=1)return 3;std::cout<<b::ToHex(b::Sha256(bundle))<<'\n';return 0;}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 2;}
}
