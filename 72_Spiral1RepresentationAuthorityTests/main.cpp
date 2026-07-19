#include "../00_Foundation/Base.h"
#include "../60_PgaRigidTransformSemantic/PgaRigidTransformSemantic.h"
#include "../61_Spiral1Contracts/Spiral1Contracts.h"
#include "../62_Spiral1Corpus/Spiral1Corpus.h"
#include "../63_D3D12RepresentationCandidate/D3D12RepresentationCandidate.h"
#include "../64_D3D12RepresentationPlanner/D3D12RepresentationPlanner.h"
#include "../65_D3D12RepresentationVerifier/D3D12RepresentationVerifier.h"

#include <filesystem>
#include <iostream>
#include <span>
#include <string>
#include <type_traits>

namespace
{
namespace base=sge4_5::base; namespace sem=sge4_5::spiral1::semantic; namespace con=sge4_5::spiral1::contracts; namespace cor=sge4_5::spiral1::corpus; namespace rep=sge4_5::spiral1::representation;
base::Digest256 D(std::string_view s){return base::Sha256(std::as_bytes(std::span(s.data(),s.size())));} 
rep::RepresentationTargetProfileV1 Profile(std::string_view suffix="A"){return rep::BuildRepresentationTargetProfileV1(D(std::string("Compiler-")+std::string(suffix)),D(std::string("Matrix-")+std::string(suffix)),D(std::string("Direct-")+std::string(suffix))).Value();}
sem::ApplyPgaMotorSemanticV1 Semantic(const cor::QualificationCaseV1& c){return sem::BuildApplyPgaMotorSemanticV1(c.motor,static_cast<std::uint32_t>(c.inputPoints.size()),c.caseIdentity,con::ObservationContractIdentityV2()).Value();}
void ResealRaw(rep::RawRepresentationCandidateV1& c){c.constantPayloadDigest=base::Sha256(c.constantPayload);c.candidateIdentity=rep::ComputeRawRepresentationCandidateIdentityV1(c);} 
bool Reject(const sem::ApplyPgaMotorSemanticV1&s,const rep::RepresentationTargetProfileV1&p,const rep::RawRepresentationCandidateV1&c){return !rep::IndependentRepresentationVerifierV1::Verify(s,p,c);} 
int Run(){
 static_assert(!std::is_default_constructible_v<rep::VerifiedRepresentationPlanV1>);
 static_assert(!std::is_constructible_v<rep::VerifiedRepresentationPlanV1,rep::RawRepresentationCandidateV1,base::Digest256,base::Digest256>);
 static_assert(!std::is_convertible_v<rep::RawRepresentationCandidateV1,rep::VerifiedRepresentationPlanV1>);
 auto corpus=cor::BuildQualificationCorpusV1();if(!corpus)return 1;auto profile=Profile();auto s=Semantic(corpus.Value().cases[7]);auto all=rep::PlanAllRepresentationCandidatesV1(s,profile).Value();
 auto vm=rep::IndependentRepresentationVerifierV1::Verify(s,profile,all[0]);auto vp=rep::IndependentRepresentationVerifierV1::Verify(s,profile,all[1]);if(!vm||!vp)return 2;
 if(!rep::IndependentRepresentationVerifierV1::ValidatePlanContext(vm.Value(),s,profile))return 3;
 auto copied=vm.Value();if(rep::SerializeVerifiedRepresentationPlanV1(copied)!=rep::SerializeVerifiedRepresentationPlanV1(vm.Value()))return 4;
 auto mutated=all[0];mutated.constantPayload[0]^=std::byte{1};ResealRaw(mutated);if(!rep::ValidateRawRepresentationCandidateStructureV1(mutated)||!Reject(s,profile,mutated))return 5;
 mutated=all[1];std::swap(mutated.constantPayload[0],mutated.constantPayload[4]);ResealRaw(mutated);if(!Reject(s,profile,mutated))return 6;
 mutated=all[0];mutated.sourceDigest=D("substitute");mutated.candidateIdentity=rep::ComputeRawRepresentationCandidateIdentityV1(mutated);if(!Reject(s,profile,mutated))return 7;
 mutated=all[0];mutated.programBinaryDigest=D("binary-substitute");mutated.candidateIdentity=rep::ComputeRawRepresentationCandidateIdentityV1(mutated);if(!Reject(s,profile,mutated))return 8;
 mutated=all[0];mutated.reflectionInterfaceDigest=D("reflection-substitute");mutated.candidateIdentity=rep::ComputeRawRepresentationCandidateIdentityV1(mutated);if(!Reject(s,profile,mutated))return 9;
 mutated=all[0];mutated.bindingLayoutDigest=D("binding-substitute");mutated.candidateIdentity=rep::ComputeRawRepresentationCandidateIdentityV1(mutated);if(!Reject(s,profile,mutated))return 10;
 mutated=all[0];mutated.observationContractIdentity=D("other-observation");mutated.candidateIdentity=rep::ComputeRawRepresentationCandidateIdentityV1(mutated);if(!Reject(s,profile,mutated))return 11;
 mutated=all[0];mutated.inputStrideBytes=32;mutated.candidateIdentity=rep::ComputeRawRepresentationCandidateIdentityV1(mutated);if(!Reject(s,profile,mutated))return 12;
 mutated=all[0];mutated.dispatchX+=1;mutated.candidateIdentity=rep::ComputeRawRepresentationCandidateIdentityV1(mutated);if(!Reject(s,profile,mutated))return 13;
 mutated=all[0];mutated.constantPayload[55]=std::byte{1};ResealRaw(mutated);if(!Reject(s,profile,mutated))return 14;
 mutated=all[0];mutated.candidateIdentity=D("self-reported-verified");if(!Reject(s,profile,mutated))return 15;
 auto otherS=Semantic(corpus.Value().cases[8]);if(rep::IndependentRepresentationVerifierV1::ValidatePlanContext(vm.Value(),otherS,profile))return 16;
 auto profileB=Profile("B");if(rep::IndependentRepresentationVerifierV1::ValidatePlanContext(vm.Value(),s,profileB))return 17;
 if(!Reject(s,profileB,all[0]))return 18;
 if(vm.Value().Candidate().candidateIdentity==vp.Value().Candidate().candidateIdentity||vm.Value().SealDigest()==vp.Value().SealDigest())return 19;
 std::cout<<"Stage 07 independent representation authority tests passed.\n"; return 0;
}
std::vector<std::byte> Bundle(){auto corpus=cor::BuildQualificationCorpusV1().Value();auto profile=Profile();base::BinaryWriter w;w.WriteBytes(rep::SerializeRepresentationTargetProfileV1(profile));for(const auto& c:corpus.cases){auto s=Semantic(c);auto all=rep::PlanAllRepresentationCandidatesV1(s,profile).Value();for(const auto& x:all){auto v=rep::IndependentRepresentationVerifierV1::Verify(s,profile,x).Value();auto b=rep::SerializeVerifiedRepresentationPlanV1(v);w.WriteU32(static_cast<std::uint32_t>(b.size()));w.WriteBytes(b);}}return std::move(w).Take();}
}
int main(int argc,char**argv){if(argc==3&&(std::string(argv[1])=="--emit-stage07"||std::string(argv[1])=="--emit-cu2")){auto b=Bundle();auto p=std::filesystem::path(argv[2]);if(!p.parent_path().empty())std::filesystem::create_directories(p.parent_path());auto r=base::WriteAllBytes(p,b);if(!r)return 90;std::cout<<p.string()<<" bytes="<<b.size()<<" sha256="<<base::ToHex(base::Sha256(b))<<'\n';return 0;}return Run();}
