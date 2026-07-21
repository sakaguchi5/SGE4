#include "Spiral3Corpus.h"
#include <array>
#include <bit>
#include <cmath>
#include <span>

namespace sge4_5::spiral3::corpus
{
namespace
{
struct Dq final{std::array<double,4> r{},d{};};
std::array<double,4> Mul(const std::array<double,4>&a,const std::array<double,4>&b){return {a[0]*b[0]-a[1]*b[1]-a[2]*b[2]-a[3]*b[3],a[0]*b[1]+a[1]*b[0]+a[2]*b[3]-a[3]*b[2],a[0]*b[2]-a[1]*b[3]+a[2]*b[0]+a[3]*b[1],a[0]*b[3]+a[1]*b[2]-a[2]*b[1]+a[3]*b[0]};}
std::array<double,4> Conj(const std::array<double,4>&q){return {q[0],-q[1],-q[2],-q[3]};}
Dq Compose(const Dq&p,const Dq&l){Dq o;o.r=Mul(p.r,l.r);const auto a=Mul(p.r,l.d),b=Mul(p.d,l.r);for(std::size_t i=0;i<4;++i)o.d[i]=a[i]+b[i];return o;}
ObservedPointV1 Apply(const Dq&m,const LocalPointV1&p){const std::array<double,4> q{0,p.x,p.y,p.z};const auto r=Mul(Mul(m.r,q),Conj(m.r));const auto t=Mul(m.d,Conj(m.r));return {static_cast<float>(r[1]+2*t[1]),static_cast<float>(r[2]+2*t[2]),static_cast<float>(r[3]+2*t[3]),1.0f};}
LocalPointV1 Point(std::uint32_t i)
{
    static const std::array<LocalPointV1,5> probes{{{0,0,0,1},{1,0,0,1},{0,1,0,1},{0,0,1,1},{0.37f,-0.61f,1.13f,1}}};
    if(i<probes.size())return probes[i];
    const auto sx=static_cast<std::int32_t>((i*17u+3u)%257u)-128;
    const auto sy=static_cast<std::int32_t>((i*31u+7u)%263u)-131;
    const auto sz=static_cast<std::int32_t>((i*47u+11u)%269u)-134;
    return {static_cast<float>(sx)/32.0f,static_cast<float>(sy)/48.0f,static_cast<float>(sz)/64.0f,1.0f};
}
}
base::Result<spiral2::hierarchy::RigidHierarchySemanticV1,std::string> BuildCanonicalMeasurementHierarchyV1()
{
    const std::array<std::int32_t,8> parents{-1,0,1,2,3,4,5,6};
    return spiral2::hierarchy::BuildRigidHierarchySemanticV1(parents);
}
base::Result<std::vector<LocalPointV1>,std::string> BuildLocalPointCorpusV1(contracts::BoneCountV1 boneCount,contracts::ReuseCountV1 reuseCount)
{
    auto shape=contracts::BuildFixedReuseWorkloadShapeV1(boneCount,reuseCount);
    if(!shape)return base::Result<std::vector<LocalPointV1>,std::string>::Failure(shape.Error());
    const auto count=shape.Value().pointCount.Value();
    std::vector<LocalPointV1> out;out.reserve(count);
    for(std::uint32_t bone=0;bone<boneCount.Value();++bone)
        for(std::uint32_t i=0;i<reuseCount.Value();++i)
            out.push_back(Point(i));
    return base::Result<std::vector<LocalPointV1>,std::string>::Success(std::move(out));
}
std::vector<std::byte> SerializePointsV1(std::span<const LocalPointV1> points)
{
    base::BinaryWriter w;for(const auto&p:points){w.WriteU32(std::bit_cast<std::uint32_t>(p.x==0?0.0f:p.x));w.WriteU32(std::bit_cast<std::uint32_t>(p.y==0?0.0f:p.y));w.WriteU32(std::bit_cast<std::uint32_t>(p.z==0?0.0f:p.z));w.WriteU32(std::bit_cast<std::uint32_t>(p.w));}return std::move(w).Take();
}
base::Digest256 LocalPointCorpusIdentityV1(std::span<const LocalPointV1> points){return base::Sha256(SerializePointsV1(points));}
base::Result<std::vector<ReuseCaseV1>,std::string> BuildReuseCorpusV1(const spiral2::hierarchy::RigidHierarchySemanticV1& hierarchy)
{
    auto valid=spiral2::hierarchy::ValidateRigidHierarchySemanticV1(hierarchy);if(!valid)return base::Result<std::vector<ReuseCaseV1>,std::string>::Failure(valid.Error());
    std::vector<ReuseCaseV1> out;
    for(const auto reuse:contracts::CanonicalReuseCountsV1){auto points=BuildLocalPointCorpusV1(contracts::BoneCountV1{hierarchy.boneCount},contracts::ReuseCountV1{reuse});if(!points)return base::Result<std::vector<ReuseCaseV1>,std::string>::Failure(points.Error());const auto id=LocalPointCorpusIdentityV1(points.Value());auto sem=semantic::BuildReuseSweepSemanticV1(hierarchy,contracts::ReuseCountV1{reuse},id);if(!sem)return base::Result<std::vector<ReuseCaseV1>,std::string>::Failure(sem.Error());out.push_back({"R"+std::to_string(reuse),reuse,std::move(points.Value()),std::move(sem.Value())});}
    return base::Result<std::vector<ReuseCaseV1>,std::string>::Success(std::move(out));
}
base::Result<std::vector<ObservedPointV1>,std::string> BuildIndependentCpuReferenceV1(const spiral2::hierarchy::RigidHierarchySemanticV1& h,const spiral2::contracts::DynamicLocalMotorPaletteV1& palette,std::span<const LocalPointV1> points,contracts::ReuseCountV1 reuseCount)
{
    auto hv=spiral2::hierarchy::ValidateRigidHierarchySemanticV1(h);if(!hv)return base::Result<std::vector<ObservedPointV1>,std::string>::Failure(hv.Error());auto pv=spiral2::contracts::ValidateDynamicLocalMotorPaletteV1(palette,h.boneCount);if(!pv)return base::Result<std::vector<ObservedPointV1>,std::string>::Failure(pv.Error());if(reuseCount.Value()==0||points.size()!=static_cast<std::size_t>(h.boneCount)*reuseCount.Value())return base::Result<std::vector<ObservedPointV1>,std::string>::Failure("reference point shape mismatch");
    std::vector<Dq> global(h.boneCount);for(const auto bone:h.canonicalBoneOrder){Dq local;for(std::size_t i=0;i<4;++i){local.r[i]=palette.motors[bone].qr[i];local.d[i]=palette.motors[bone].qd[i];}const auto parent=h.parentIndices[bone];global[bone]=parent<0?local:Compose(global[static_cast<std::uint32_t>(parent)],local);}
    std::vector<ObservedPointV1> out;out.reserve(points.size());for(std::uint32_t bone=0;bone<h.boneCount;++bone)for(std::uint32_t i=0;i<reuseCount.Value();++i)out.push_back(Apply(global[bone],points[static_cast<std::size_t>(bone)*reuseCount.Value()+i]));return base::Result<std::vector<ObservedPointV1>,std::string>::Success(std::move(out));
}
base::Result<std::vector<std::byte>,std::string> BuildCorpusFoundationBundleV1()
{
    auto hierarchy=BuildCanonicalMeasurementHierarchyV1();if(!hierarchy)return base::Result<std::vector<std::byte>,std::string>::Failure(hierarchy.Error());auto cases=BuildReuseCorpusV1(hierarchy.Value());if(!cases)return base::Result<std::vector<std::byte>,std::string>::Failure(cases.Error());auto frames=spiral2::corpus::BuildFrameCorpusV1(hierarchy.Value());if(!frames)return base::Result<std::vector<std::byte>,std::string>::Failure(frames.Error());base::BinaryWriter w;w.WriteU32(1);w.WriteBytes(contracts::CanonicalReuseSetIdentityV1());w.WriteU32(static_cast<std::uint32_t>(cases.Value().size()));w.WriteU32(static_cast<std::uint32_t>(frames.Value().size()));for(const auto& c:cases.Value()){const auto sem=semantic::SerializeReuseSweepSemanticV1(c.semantic),points=SerializePointsV1(c.localPoints);w.WriteU32(c.reuseCount);w.WriteU32(static_cast<std::uint32_t>(sem.size()));w.WriteBytes(sem);w.WriteU32(static_cast<std::uint32_t>(points.size()));w.WriteBytes(points);for(const auto& f:frames.Value()){auto ref=BuildIndependentCpuReferenceV1(hierarchy.Value(),f.palette,c.localPoints,contracts::ReuseCountV1{c.reuseCount});if(!ref)return base::Result<std::vector<std::byte>,std::string>::Failure(ref.Error());const auto palette=spiral2::contracts::SerializeDynamicLocalMotorPaletteV1(f.palette),reference=SerializePointsV1(ref.Value());w.WriteBytes(base::Sha256(palette));w.WriteBytes(base::Sha256(reference));}}return base::Result<std::vector<std::byte>,std::string>::Success(std::move(w).Take());
}
}
