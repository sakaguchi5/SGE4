#include "../00_Foundation/Base.h"
#include "../60_PgaRigidTransformSemantic/PgaRigidTransformSemantic.h"
#include "../61_Spiral1Contracts/Spiral1Contracts.h"
#include "../62_Spiral1Corpus/Spiral1Corpus.h"
#include "../63_D3D12RepresentationCandidate/D3D12RepresentationCandidate.h"
#include "../64_D3D12RepresentationPlanner/D3D12RepresentationPlanner.h"

#include <bit>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <span>
#include <string>

namespace
{
namespace base=sge4_5::base; namespace sem=sge4_5::spiral1::semantic; namespace con=sge4_5::spiral1::contracts;
namespace cor=sge4_5::spiral1::corpus; namespace rep=sge4_5::spiral1::representation;
base::Digest256 D(std::string_view s){return base::Sha256(std::as_bytes(std::span(s.data(),s.size())));} 
rep::RepresentationTargetProfileV1 Profile(){return rep::BuildRepresentationTargetProfileV1(D("CU2.CompilerFixture.V1"),D("CU2.MatrixBinaryFixture.V1"),D("CU2.DirectBinaryFixture.V1")).Value();}
sem::ApplyPgaMotorSemanticV1 Semantic(const cor::QualificationCaseV1& c){return sem::BuildApplyPgaMotorSemanticV1(c.motor,static_cast<std::uint32_t>(c.inputPoints.size()),c.caseIdentity,con::ObservationContractIdentityV2()).Value();}
float F(std::span<const std::byte> b,std::size_t o){std::uint32_t x=std::to_integer<std::uint8_t>(b[o])|(std::to_integer<std::uint8_t>(b[o+1])<<8)|(std::to_integer<std::uint8_t>(b[o+2])<<16)|(std::to_integer<std::uint8_t>(b[o+3])<<24);return std::bit_cast<float>(x);} 
int Run(){
 auto corpus=cor::BuildQualificationCorpusV1(); if(!corpus){std::cerr<<corpus.Error()<<'\n';return 1;} auto profile=Profile();
 const auto& c=corpus.Value().cases[7]; auto semantic=Semantic(c); auto all=rep::PlanAllRepresentationCandidatesV1(semantic,profile); if(!all){std::cerr<<all.Error()<<'\n';return 2;}
 const auto& m=all.Value()[0]; const auto& p=all.Value()[1];
 if(m.semanticIdentity!=p.semanticIdentity||m.candidateIdentity==p.candidateIdentity||m.templateIdentity==p.templateIdentity)return 3;
 if(m.constantPayload.size()!=64||p.constantPayload.size()!=48||m.pointCount!=c.inputPoints.size()||p.pointCount!=c.inputPoints.size())return 4;
 if(m.inputSchemaIdentity!=p.inputSchemaIdentity||m.outputSchemaIdentity!=p.outputSchemaIdentity||m.observationContractIdentity!=p.observationContractIdentity)return 5;
 const auto point=c.inputPoints[0]; const float x=F(m.constantPayload,0)*point.x+F(m.constantPayload,4)*point.y+F(m.constantPayload,8)*point.z+F(m.constantPayload,12);
 const float y=F(m.constantPayload,16)*point.x+F(m.constantPayload,20)*point.y+F(m.constantPayload,24)*point.z+F(m.constantPayload,28);
 const float z=F(m.constantPayload,32)*point.x+F(m.constantPayload,36)*point.y+F(m.constantPayload,40)*point.z+F(m.constantPayload,44);
 if(!con::WithinReferenceTolerance(x,c.referencePoints[0].x)||!con::WithinReferenceTolerance(y,c.referencePoints[0].y)||!con::WithinReferenceTolerance(z,c.referencePoints[0].z))return 6;
 for(std::size_t i=0;i<4;++i)if(F(p.constantPayload,i*4)!=static_cast<float>(c.motor.Qr()[i]))return 7;
 for(std::size_t i=0;i<4;++i)if(F(p.constantPayload,16+i*4)!=static_cast<float>(c.motor.Qd()[i]))return 8;
 if(rep::CanonicalProgramTemplateSourceV1(rep::LoweringKindV1::DirectPgaMotorCompute).find("row0")!=std::string_view::npos)return 9;
 if(!rep::ValidateRawRepresentationCandidateStructureV1(m)||!rep::ValidateRawRepresentationCandidateStructureV1(p))return 10;
 if(rep::SerializeRawRepresentationCandidateV1(m)!=rep::SerializeRawRepresentationCandidateV1(m))return 11;
 auto badProfile=rep::BuildRepresentationTargetProfileV1({},D("a"),D("b")); if(badProfile)return 12;
 const sem::Float4Point conditionedReference{-7322.333f,-6.774168f,3933.261f,1.0f};
 const sem::Float4Point conditionedMatrix{-7322.333f,-6.7744140625f,3933.26123046875f,1.0f};
 const sem::Float4Point conditionedPga{-7322.333f,-6.7744140625f,3933.261f,1.0f};
 const auto legacyRecord=con::BuildComparisonRecordV1(28,conditionedReference,conditionedMatrix,conditionedPga);
 const auto conditionedRecord=con::BuildComparisonRecordV2(28,conditionedReference,conditionedMatrix,conditionedPga);
 if(legacyRecord.mismatchFlags==0)return 15;
 if(conditionedRecord.mismatchFlags!=0)return 16;
 std::cout<<"Stage 06 representation candidate generation tests passed.\n"; return 0;
}
std::vector<std::byte> Bundle(){auto corpus=cor::BuildQualificationCorpusV1().Value();auto profile=Profile();base::BinaryWriter w;w.WriteBytes(rep::SerializeRepresentationTargetProfileV1(profile));for(const auto& c:corpus.cases){auto s=Semantic(c);auto all=rep::PlanAllRepresentationCandidatesV1(s,profile).Value();for(const auto& x:all){auto b=rep::SerializeRawRepresentationCandidateV1(x);w.WriteU32(static_cast<std::uint32_t>(b.size()));w.WriteBytes(b);}}return std::move(w).Take();}
}
int main(int argc,char**argv){if(argc==3&&std::string(argv[1])=="--emit-stage06"){auto b=Bundle();auto p=std::filesystem::path(argv[2]);if(!p.parent_path().empty())std::filesystem::create_directories(p.parent_path());auto r=base::WriteAllBytes(p,b);if(!r)return 90;std::cout<<p.string()<<" bytes="<<b.size()<<" sha256="<<base::ToHex(base::Sha256(b))<<'\n';return 0;}return Run();}
