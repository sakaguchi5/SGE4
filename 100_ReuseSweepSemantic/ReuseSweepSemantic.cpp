#include "ReuseSweepSemantic.h"
#include <span>
#include <string_view>

namespace sge4_5::spiral3::semantic
{
namespace
{
base::Digest256 Lit(std::string_view value){return base::Sha256(std::as_bytes(std::span(value.data(),value.size())));}
}
base::Digest256 ConsumerMeaningIdentityV1(){return Lit("SGE4-5.Spiral3.FixedReuseRigidConsumerMeaning.V1");}
base::Digest256 LocalPointSchemaIdentityV1(){return Lit("SGE4-5.Spiral3.LocalPoint.Float4.BoneMajor.V1");}
base::Digest256 ObservationContractIdentityV1(){return Lit("SGE4-5.Spiral3.ReuseObservationContract.V1");}

base::Result<ReuseSweepSemanticV1,std::string> BuildReuseSweepSemanticV1(
    const spiral2::hierarchy::RigidHierarchySemanticV1& hierarchy,
    contracts::ReuseCountV1 reuseCount,
    const base::Digest256& localPointCorpusIdentity)
{
    auto valid=spiral2::hierarchy::ValidateRigidHierarchySemanticV1(hierarchy);
    if(!valid)return base::Result<ReuseSweepSemanticV1,std::string>::Failure(valid.Error());
    if(localPointCorpusIdentity==base::Digest256{})return base::Result<ReuseSweepSemanticV1,std::string>::Failure("local point corpus identity is missing");
    auto shape=contracts::BuildFixedReuseWorkloadShapeV1(
        contracts::BoneCountV1{hierarchy.boneCount},reuseCount);
    if(!shape)return base::Result<ReuseSweepSemanticV1,std::string>::Failure(shape.Error());
    ReuseSweepSemanticV1 out;
    out.hierarchySemanticIdentity=spiral2::hierarchy::RigidHierarchySemanticIdentityV1(hierarchy);
    out.consumerMeaningIdentity=ConsumerMeaningIdentityV1();
    out.localPointSchemaIdentity=LocalPointSchemaIdentityV1();
    out.localPointCorpusIdentity=localPointCorpusIdentity;
    out.observationContractIdentity=ObservationContractIdentityV1();
    out.boneCount=shape.Value().boneCount.Value();
    out.reuseCount=shape.Value().reuseCount.Value();
    out.pointCount=shape.Value().pointCount.Value();
    return base::Result<ReuseSweepSemanticV1,std::string>::Success(out);
}
base::Result<void,std::string> ValidateReuseSweepSemanticV1(
    const spiral2::hierarchy::RigidHierarchySemanticV1& hierarchy,
    const ReuseSweepSemanticV1& value)
{
    auto hierarchyValid=spiral2::hierarchy::ValidateRigidHierarchySemanticV1(hierarchy);
    if(!hierarchyValid)return hierarchyValid;
    if(value.hierarchySemanticIdentity!=spiral2::hierarchy::RigidHierarchySemanticIdentityV1(hierarchy))return base::Result<void,std::string>::Failure("hierarchy semantic identity mismatch");
    if(value.consumerMeaningIdentity!=ConsumerMeaningIdentityV1())return base::Result<void,std::string>::Failure("consumer meaning identity mismatch");
    if(value.localPointSchemaIdentity!=LocalPointSchemaIdentityV1())return base::Result<void,std::string>::Failure("local point schema identity mismatch");
    if(value.localPointCorpusIdentity==base::Digest256{})return base::Result<void,std::string>::Failure("local point corpus identity is missing");
    if(value.observationContractIdentity!=ObservationContractIdentityV1())return base::Result<void,std::string>::Failure("observation contract identity mismatch");
    if(value.boneCount!=hierarchy.boneCount||value.reuseCount==0)return base::Result<void,std::string>::Failure("bone or reuse count is invalid");
    const std::uint64_t expected=static_cast<std::uint64_t>(value.boneCount)*value.reuseCount;
    if(expected!=value.pointCount||expected>contracts::MaximumConsumerPointCountV1)return base::Result<void,std::string>::Failure("consumer point count is invalid");
    return base::Result<void,std::string>::Success();
}
std::vector<std::byte> SerializeReuseSweepSemanticV1(const ReuseSweepSemanticV1& value)
{
    base::BinaryWriter writer;
    writer.WriteU32(ReuseSweepSemanticVersionV1);writer.WriteU32(0);
    writer.WriteBytes(value.hierarchySemanticIdentity);writer.WriteBytes(value.consumerMeaningIdentity);
    writer.WriteBytes(value.localPointSchemaIdentity);writer.WriteBytes(value.localPointCorpusIdentity);
    writer.WriteBytes(value.observationContractIdentity);
    writer.WriteU32(value.boneCount);writer.WriteU32(value.reuseCount);writer.WriteU32(value.pointCount);writer.WriteU32(0);
    return std::move(writer).Take();
}
base::Digest256 ReuseSweepSemanticIdentityV1(const ReuseSweepSemanticV1& value){return base::Sha256(SerializeReuseSweepSemanticV1(value));}
}
