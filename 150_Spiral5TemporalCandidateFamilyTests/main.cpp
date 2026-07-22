#include "../00_Foundation/BinaryIO.h"
#include "../00_Foundation/FileIO.h"
#include "../00_Foundation/Sha256.h"
#include "../134_TemporalStateSemantic/TemporalStateSemantic.h"
#include "../136_Spiral5TemporalExecution/Spiral5TemporalExecution.h"
#include "../147_TemporalCandidateFamily/TemporalCandidateFamily.h"
#include "../148_TemporalCandidateFamilyPlanner/TemporalCandidateFamilyPlanner.h"
#include "../149_TemporalCandidateFamilyVerifier/TemporalCandidateFamilyVerifier.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace base=sge4_5::base;
namespace sem=sge4_5::spiral5::semantic;
namespace cand=sge4_5::spiral5::family_candidate;
namespace plan=sge4_5::spiral5::family_planner;
namespace verify=sge4_5::spiral5::family_verification;
namespace exec=sge4_5::spiral5::execution;

static_assert(!std::is_default_constructible_v<verify::VerifiedTemporalCandidateV1>);
static_assert(!std::is_convertible_v<cand::RawTemporalCandidateV1,verify::VerifiedTemporalCandidateV1>);
static_assert(!std::is_default_constructible_v<exec::FrozenVerifiedTemporalCandidateV1>);
static_assert(!std::is_convertible_v<cand::RawTemporalCandidateV1,exec::FrozenVerifiedTemporalCandidateV1>);

namespace
{
void Require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
void Toggle(base::Digest256& value){value[0]^=std::byte{1};}
std::vector<std::byte> ReadFile(const std::filesystem::path& path)
{
    std::ifstream stream(path,std::ios::binary);
    if(!stream)throw std::runtime_error("could not reopen evidence");
    stream.seekg(0,std::ios::end);const auto size=stream.tellg();stream.seekg(0,std::ios::beg);
    std::vector<std::byte> bytes(static_cast<std::size_t>(size));
    stream.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
    if(!stream)throw std::runtime_error("could not read evidence");
    return bytes;
}
void WriteBundle(
    const std::filesystem::path& path,
    const sem::TemporalStateSemanticV1& semantic,
    const std::vector<sem::UpdateScheduleV1>& schedules,
    const std::vector<std::vector<cand::RawTemporalCandidateV1>>& raw,
    const std::vector<std::vector<verify::VerifiedTemporalCandidateV1>>& verified,
    const std::vector<std::vector<exec::FrozenVerifiedTemporalCandidateV1>>& frozen,
    const std::vector<exec::TemporalCandidateFamilyArchitectureResultV1>& architecture)
{
    base::BinaryWriter writer;writer.WriteU32(0x34433553u);writer.WriteU32(1);
    const auto semanticBytes=sem::SerializeTemporalStateSemanticV1(semantic);
    writer.WriteU32(static_cast<std::uint32_t>(semanticBytes.size()));writer.WriteBytes(semanticBytes);
    writer.WriteU32(static_cast<std::uint32_t>(schedules.size()));
    for(std::size_t s=0;s<schedules.size();++s)
    {
        const auto scheduleBytes=sem::SerializeUpdateScheduleV1(schedules[s]);
        writer.WriteU32(static_cast<std::uint32_t>(scheduleBytes.size()));writer.WriteBytes(scheduleBytes);
        writer.WriteU32(static_cast<std::uint32_t>(raw[s].size()));
        for(std::size_t i=0;i<raw[s].size();++i)
        {
            const auto r=cand::SerializeRawTemporalCandidateV1(raw[s][i]);
            const auto v=verify::SerializeVerifiedTemporalCandidateV1(verified[s][i]);
            const auto f=exec::SerializeFrozenVerifiedTemporalCandidateV1(frozen[s][i]);
            writer.WriteU32(static_cast<std::uint32_t>(r.size()));writer.WriteBytes(r);writer.WriteBytes(raw[s][i].rawCandidateIdentity);
            writer.WriteU32(static_cast<std::uint32_t>(v.size()));writer.WriteBytes(v);
            writer.WriteU32(static_cast<std::uint32_t>(f.size()));writer.WriteBytes(f);
        }
        const auto a=exec::SerializeTemporalCandidateFamilyArchitectureEvidenceV1(architecture[s]);
        writer.WriteU32(static_cast<std::uint32_t>(a.size()));writer.WriteBytes(a);
    }
    writer.WriteBytes(base::Sha256(writer.Bytes()));
    std::filesystem::create_directories(path.parent_path());
    auto written=base::WriteAllBytes(path,std::move(writer).Take());
    if(!written)throw std::runtime_error(written.Error());
}
}

int main(int argc,char** argv)
{
    try
    {
        std::filesystem::path output="build/tests/spiral5-cu4/evidence.bin";
        for(int i=1;i<argc;++i){const std::string arg=argv[i];if(arg=="--emit"&&i+1<argc)output=argv[++i];else throw std::runtime_error("unknown argument");}
        const auto semantic=sem::BuildCanonicalTemporalStateSemanticV1();
        const auto context=verify::BuildCanonicalTemporalCandidateFamilyVerificationContextV1();
        std::vector<sem::UpdateScheduleV1> schedules;
        for(const std::uint32_t interval:{1u,4u,64u})
        {auto value=sem::BuildPeriodicUpdateScheduleV1(interval);Require(static_cast<bool>(value),"periodic schedule failed");schedules.push_back(std::move(value).Value());}
        auto irregular=sem::BuildNamedUpdateScheduleV1("Irregular");Require(static_cast<bool>(irregular),"Irregular schedule failed");schedules.push_back(std::move(irregular).Value());

        std::vector<std::vector<cand::RawTemporalCandidateV1>> allRaw;
        std::vector<std::vector<verify::VerifiedTemporalCandidateV1>> allVerified;
        std::vector<std::vector<exec::FrozenVerifiedTemporalCandidateV1>> allFrozen;
        std::vector<exec::TemporalCandidateFamilyArchitectureResultV1> allArchitecture;
        std::uint32_t totalInvocations=0;
        for(const auto& schedule:schedules)
        {
            auto planned=plan::PlanCanonicalTemporalCandidateFamilyV1(semantic,schedule);
            Require(static_cast<bool>(planned),"Temporal Candidate family Planner failed");
            auto raw=std::move(planned).Value();Require(raw.size()==3,"Temporal Candidate family size differs");
            std::vector<verify::VerifiedTemporalCandidateV1> verified;
            std::vector<exec::FrozenVerifiedTemporalCandidateV1> frozen;
            for(const auto& proposal:raw)
            {
                auto v=verify::VerifyTemporalCandidateV1(semantic,schedule,context,proposal);
                Require(static_cast<bool>(v),"Temporal Candidate independent verification failed");
                verified.push_back(std::move(v).Value());
                auto binding=exec::BuildCanonicalTemporalCandidateResourceBindingInputV1(verified.back(),1);
                auto f=exec::FreezeVerifiedTemporalCandidateV1(semantic,schedule,context,verified.back(),binding);
                Require(static_cast<bool>(f),"Temporal Candidate Freeze failed");
                frozen.push_back(std::move(f).Value());
            }
            auto architecture=exec::RunVerifiedTemporalCandidateFamilyOnWarpV1(semantic,schedule,context,frozen,1);
            if(!architecture)throw std::runtime_error(architecture.Error().stage+": "+architecture.Error().message);
            Require(architecture.Value().pairwiseOutputEquivalent,"Temporal Candidate pairwise output differs");
            for(const auto& candidate:architecture.Value().candidates)
            {
                Require(candidate.invocations.size()==sem::TimelineLengthV1,"Candidate Timeline length differs");
                totalInvocations+=static_cast<std::uint32_t>(candidate.invocations.size());
            }
            allRaw.push_back(std::move(raw));allVerified.push_back(std::move(verified));allFrozen.push_back(std::move(frozen));
            allArchitecture.push_back(std::move(architecture).Value());
        }
        Require(totalInvocations==4u*3u*128u,"Temporal Candidate invocation total differs");

        std::uint32_t attempted=0,rejected=0;
        auto reject=[&](cand::RawTemporalCandidateV1 proposal,bool rehash)
        {
            ++attempted;if(rehash)proposal.rawCandidateIdentity=cand::RawTemporalCandidateIdentityV1(proposal);
            if(!verify::VerifyTemporalCandidateV1(semantic,schedules[1],context,proposal))++rejected;
        };
        const auto canonical=allRaw[1][2];
        auto p=canonical;Toggle(p.semanticIdentity);reject(p,false);
        p=canonical;Toggle(p.scheduleIdentity);reject(p,false);
        p=canonical;Toggle(p.targetProfileIdentity);reject(p,false);
        p=canonical;Toggle(p.observationContractIdentity);reject(p,false);
        p=canonical;Toggle(p.completionContractIdentity);reject(p,false);
        p=canonical;Toggle(p.deviceEpochPolicyIdentity);reject(p,false);
        p=canonical;Toggle(p.spiral4IndirectArtifactIdentity);reject(p,false);
        p=canonical;Toggle(p.argumentProducerProgramIdentity);reject(p,false);
        p=canonical;Toggle(p.hierarchyProgramIdentity);reject(p,false);
        p=canonical;Toggle(p.consumerProgramIdentity);reject(p,false);
        p=canonical;Toggle(p.proposedArtifactIdentity);reject(p,false);
        p=canonical;Toggle(p.proposedHistoryResourceIdentity);reject(p,false);
        p=canonical;Toggle(p.invocationAuthorityIdentity);reject(p,false);
        p=canonical;p.kind=cand::TemporalCandidateKindV1::GlobalMotorHistoryReuse;reject(p,false);
        p=canonical;p.historyRole=cand::TemporalFamilyHistoryRoleV1::GlobalMotorHistory;reject(p,false);
        p=canonical;p.issuancePolicy=cand::TemporalIssuancePolicyV1::UpdateIndirectHierarchy;reject(p,false);
        p=canonical;p.completionPolicy=cand::TemporalCompletionPolicyV1::ConsumerCompletionRetainsGlobalMotor;reject(p,false);
        p=canonical;p.holdPolicy=cand::TemporalHoldPolicyV1::ZeroHierarchyReuseGlobalMotor;reject(p,false);
        p=canonical;++p.timelineLength;reject(p,false);p=canonical;++p.workCount;reject(p,false);p=canonical;++p.threadsPerGroup;reject(p,false);
        p=canonical;++p.historyDepth;reject(p,false);p=canonical;++p.historyBytes;reject(p,false);p=canonical;++p.argumentBufferBytes;reject(p,false);
        p=canonical;++p.argumentStrideBytes;reject(p,false);p=canonical;++p.maxCommandCount;reject(p,false);p=canonical;++p.argumentProducerDispatchX;reject(p,false);
        p=canonical;++p.hierarchyUpdateDispatchX;reject(p,false);p=canonical;++p.hierarchyHoldDispatchX;reject(p,false);
        p=canonical;++p.consumerUpdateDispatchX;reject(p,false);p=canonical;++p.consumerHoldDispatchX;reject(p,false);
        p=canonical;++p.updateCount;reject(p,false);p=canonical;++p.holdCount;reject(p,false);p=canonical;++p.maximumSourceGeneration;reject(p,false);
        p=canonical;Toggle(p.rawCandidateIdentity);reject(p,false);
        p=canonical;++p.consumerHoldDispatchX;reject(p,true);
        p=canonical;Toggle(p.proposedHistoryResourceIdentity);reject(p,true);
        p=canonical;p.historyRole=cand::TemporalFamilyHistoryRoleV1::GlobalMotorHistory;reject(p,true);
        Require(attempted==38&&rejected==attempted,"Temporal Candidate family mutation gate failed");

        Require(!verify::ValidateVerifiedTemporalCandidateContextV1(allVerified[1][0],semantic,schedules[2],context),"Cross-schedule Candidate seal replay accepted");
        auto badBinding=exec::BuildCanonicalTemporalCandidateResourceBindingInputV1(allVerified[1][2],1);Toggle(badBinding.actualHistoryResourceIdentity);
        Require(!exec::FreezeVerifiedTemporalCandidateV1(semantic,schedules[1],context,allVerified[1][2],badBinding),"Wrong History Resource accepted");
        auto roleBinding=exec::BuildCanonicalTemporalCandidateResourceBindingInputV1(allVerified[1][2],1);roleBinding.historyRole=cand::TemporalFamilyHistoryRoleV1::GlobalMotorHistory;
        Require(!exec::FreezeVerifiedTemporalCandidateV1(semantic,schedules[1],context,allVerified[1][2],roleBinding),"Cross-role History binding accepted");
        Require(!exec::ValidateFrozenVerifiedTemporalCandidateV1(allFrozen[1][2],semantic,schedules[1],context,2),"Cross-epoch Frozen Candidate accepted");

        WriteBundle(output,semantic,schedules,allRaw,allVerified,allFrozen,allArchitecture);
        std::cout<<"Spiral 5 CU4 Temporal Candidate family passed.\n";
        std::cout<<"Adapter: "<<allArchitecture.front().adapterDescription<<"\n";
        std::cout<<"Candidates: A.EveryInvocationRecompute, B.GlobalMotorHistoryReuse, C.FinalOutputHistoryReuse\n";
        std::cout<<"Schedules: P1, P4, P64, Irregular\n";
        std::cout<<"Candidate invocations: 4 x 3 x 128 = 1536\n";
        std::cout<<"Pairwise output equivalence: qualified\n";
        std::cout<<"History materialization positions: None / Global Motor / Final Output\n";
        std::cout<<"Runtime temporal-policy decision: None\n";
        std::cout<<"CU4 Candidate family evidence SHA-256: "<<base::ToHex(base::Sha256(ReadFile(output)))<<"\n";
        return 0;
    }
    catch(const std::exception& error){std::cerr<<"Spiral 5 CU4 failure: "<<error.what()<<'\n';return 1;}
}
