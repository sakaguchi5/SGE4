#include "../00_Foundation/Base.h"
#include "../82_Spiral2Corpus/Spiral2Corpus.h"
#include "../84_HierarchyRepresentationPlanner/HierarchyRepresentationPlanner.h"
#include "../85_HierarchyRepresentationVerifier/HierarchyRepresentationVerifier.h"
#include "../88_Spiral2Scenarios/Spiral2Scenarios.h"
#include <array>
#include <filesystem>
#include <iostream>

namespace b=sge4_5::base;
namespace c=sge4_5::spiral2::corpus;
namespace h=sge4_5::spiral2::hierarchy;
namespace candidate=sge4_5::spiral2::candidate;
namespace planner=sge4_5::spiral2::planner;
namespace verifier=sge4_5::spiral2::verification;
namespace scenario=sge4_5::spiral2::scenarios;

namespace
{
void Blob(b::BinaryWriter& writer,std::span<const std::byte> bytes){writer.WriteU64(bytes.size());writer.WriteBytes(bytes);}
std::vector<std::byte> BuildFreezeBundle()
{
    auto topologies=c::BuildTopologyCorpusV1();if(!topologies)throw std::runtime_error(topologies.Error());auto corpus=c::BuildCorpusFoundationBundleV1();if(!corpus)throw std::runtime_error(corpus.Error());b::BinaryWriter writer;constexpr std::string_view domain="SGE4-5.Spiral2.CU5.DeterminismFreeze.V1";writer.WriteU32(static_cast<std::uint32_t>(domain.size()));writer.WriteBytes(std::as_bytes(std::span(domain.data(),domain.size())));Blob(writer,corpus.Value());writer.WriteBytes(h::ObservationContractIdentityV2());writer.WriteU32(static_cast<std::uint32_t>(topologies.Value().size()));const std::array kinds{candidate::CandidateKindV1::MatrixHierarchy,candidate::CandidateKindV1::DirectPgaHierarchy,candidate::CandidateKindV1::HybridHierarchy};for(const auto& topology:topologies.Value()){auto semantic=h::SerializeRigidHierarchySemanticV1(topology.hierarchy);Blob(writer,semantic);for(const auto kind:kinds){auto raw=planner::PlanCandidateGraphV1(topology.hierarchy,kind);if(!raw)throw std::runtime_error(raw.Error());auto rawBytes=candidate::SerializeRawCandidateGraphV1(raw.Value());Blob(writer,rawBytes);auto verified=verifier::VerifyCandidateGraphV1(topology.hierarchy,raw.Value());if(!verified)throw std::runtime_error(verified.Error());auto verifiedBytes=verifier::SerializeVerifiedHierarchyRepresentationV1(verified.Value());Blob(writer,verifiedBytes);}auto frozen=scenario::BuildFrozenHierarchyComparisonScenarioV1(topology.hierarchy);if(!frozen)throw std::runtime_error(frozen.Error().stage+": "+frozen.Error().message);auto frozenBytes=scenario::SerializeFrozenHierarchyComparisonScenarioEvidenceV1(frozen.Value());Blob(writer,frozenBytes);auto frames=c::BuildFrameCorpusV1(topology.hierarchy);if(!frames)throw std::runtime_error(frames.Error());writer.WriteU32(static_cast<std::uint32_t>(frames.Value().size()));for(const auto& frame:frames.Value()){auto palette=sge4_5::spiral2::contracts::SerializeDynamicLocalMotorPaletteV1(frame.palette);auto reference=c::BuildIndependentCpuReferenceV1(topology.hierarchy,frame.palette);if(!reference)throw std::runtime_error(reference.Error());auto referenceBytes=c::SerializeReferencePointsV1(reference.Value());writer.WriteBytes(b::Sha256(palette));writer.WriteBytes(b::Sha256(referenceBytes));}}
    return std::move(writer).Take();
}
}

int main(int argc,char**argv)
{
    try{auto bundle=BuildFreezeBundle();if(argc==3&&std::string(argv[1])=="--emit"){auto wrote=b::WriteAllBytes(std::filesystem::path(argv[2]),bundle);if(!wrote)return 1;}std::cout<<b::ToHex(b::Sha256(bundle))<<'\n';return 0;}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 2;}
}
