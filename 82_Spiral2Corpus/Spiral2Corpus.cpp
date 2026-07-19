#include "Spiral2Corpus.h"

#include <bit>
#include <cmath>
#include <numbers>
#include <span>

namespace sge4_5::spiral2::corpus
{
namespace
{
struct Dq final { std::array<double,4> r{}, d{}; };

[[nodiscard]] std::array<double,4> Mul(const std::array<double,4>& a, const std::array<double,4>& b)
{
    return {a[0]*b[0]-a[1]*b[1]-a[2]*b[2]-a[3]*b[3],
            a[0]*b[1]+a[1]*b[0]+a[2]*b[3]-a[3]*b[2],
            a[0]*b[2]-a[1]*b[3]+a[2]*b[0]+a[3]*b[1],
            a[0]*b[3]+a[1]*b[2]-a[2]*b[1]+a[3]*b[0]};
}
[[nodiscard]] std::array<double,4> Conj(const std::array<double,4>& q) { return {q[0],-q[1],-q[2],-q[3]}; }
[[nodiscard]] Dq Compose(const Dq& parent, const Dq& local)
{
    Dq value;
    value.r = Mul(parent.r, local.r);
    const auto a=Mul(parent.r,local.d), b=Mul(parent.d,local.r);
    for(std::size_t i=0;i<4;++i) value.d[i]=a[i]+b[i];
    return value;
}
[[nodiscard]] ObservedPointV1 Apply(const Dq& motor, const ProbePointV1& point)
{
    const std::array<double,4> p{0.0,point.x,point.y,point.z};
    const auto rotated=Mul(Mul(motor.r,p),Conj(motor.r));
    const auto translation=Mul(motor.d,Conj(motor.r));
    return {static_cast<float>(rotated[1]+2.0*translation[1]),
            static_cast<float>(rotated[2]+2.0*translation[2]),
            static_cast<float>(rotated[3]+2.0*translation[3]),1.0f};
}
[[nodiscard]] std::vector<std::int32_t> Chain(std::uint32_t count)
{
    std::vector<std::int32_t> result(count,-1);
    for(std::uint32_t i=1;i<count;++i) result[i]=static_cast<std::int32_t>(i-1);
    return result;
}
[[nodiscard]] std::vector<std::int32_t> Star(std::uint32_t count)
{
    std::vector<std::int32_t> result(count,0); result[0]=-1; return result;
}
[[nodiscard]] std::vector<std::int32_t> Balanced(std::uint32_t count)
{
    std::vector<std::int32_t> result(count,-1);
    for(std::uint32_t i=1;i<count;++i) result[i]=static_cast<std::int32_t>((i-1)/2);
    return result;
}
[[nodiscard]] std::vector<std::int32_t> Mixed(std::uint32_t count)
{
    std::vector<std::int32_t> result(count,-1);
    for(std::uint32_t i=1;i<count;++i)
        result[i]=static_cast<std::int32_t>((i%7==0) ? 0u : (i%5==0 ? (i-1)/3 : i-1));
    return result;
}
[[nodiscard]] contracts::RotationTranslationV1 MakeInput(
    std::uint32_t frame, std::uint32_t bone, std::uint32_t count, std::uint64_t& state)
{
    double ax=1.0,ay=0.0,az=0.0,angle=0.0,tx=0.0,ty=0.0,tz=0.0;
    const double b=static_cast<double>(bone+1);
    switch(frame)
    {
    case 0: break;
    case 1: tx=0.01*b; ty=-0.02*b; tz=0.005*b; break;
    case 2: az=1.0; ax=0.0; angle=0.015*b; break;
    case 3: ax=bone%3==0; ay=bone%3==1; az=bone%3==2; angle=0.011*b; break;
    case 4: ax=1.0; ay=2.0; az=-1.0; angle=0.007*b; tx=0.02*b; ty=0.003*b; tz=-0.01*b; break;
    case 5: ax=1.0; ay=-1.0; az=2.0; angle=1.0e-7*b; break;
    case 6: ax=bone%2?1.0:0.0; ay=bone%2?0.0:1.0; angle=std::numbers::pi-1.0e-5*b/static_cast<double>(count); break;
    case 7:
        state=state*6364136223846793005ull+1442695040888963407ull;
        ax=static_cast<double>((state>>8)&255)-127.0; ay=static_cast<double>((state>>16)&255)-127.0; az=static_cast<double>((state>>24)&255)-127.0;
        if(ax==0&&ay==0&&az==0) ax=1; angle=(static_cast<double>((state>>32)&65535)/65535.0-0.5)*0.8;
        tx=(static_cast<double>((state>>12)&1023)-511.0)/5110.0; ty=-tx*0.5; tz=tx*0.25; break;
    case 8: ay=1.0; ax=0.0; angle=0.0025; break;
    case 9: tx=0.003; ty=-0.001; tz=0.002; break;
    case 10: ax=1.0; ay=3.0; az=2.0; angle=0.02; tx=(bone%2?1.0e2:1.0e-3); ty=-tx*0.25; tz=tx*0.125; break;
    case 11: ax=-1.0; ay=bone%2?1.0e-12:0.0; az=0.0; angle=bone%2?std::numbers::pi:0.0; break;
    default: break;
    }
    const double norm=std::sqrt(ax*ax+ay*ay+az*az);
    const double half=angle*0.5, scale=std::sin(half)/norm;
    return {std::cos(half),ax*scale,ay*scale,az*scale,tx,ty,tz};
}
}

const std::array<ProbePointV1,5>& CanonicalProbesV1()
{
    static const std::array<ProbePointV1,5> value{{{0,0,0,1},{1,0,0,1},{0,1,0,1},{0,0,1,1},{0.37,-0.61,1.13,1}}};
    return value;
}

base::Result<std::vector<TopologyCaseV1>, std::string> BuildTopologyCorpusV1()
{
    const std::array<std::pair<const char*,std::vector<std::int32_t>>,9> definitions{{
        {"H00",Chain(1)},{"H01",Chain(2)},{"H02",Star(8)},{"H03",Chain(8)},
        {"H04",Balanced(15)},{"H05",Mixed(32)},{"H06",Balanced(64)},
        {"H07",Chain(64)},{"H08",Mixed(128)}}};
    std::vector<TopologyCaseV1> result;
    for(const auto& [id,parents]:definitions)
    {
        auto hierarchy=hierarchy::BuildRigidHierarchySemanticV1(parents);
        if(!hierarchy) return base::Result<std::vector<TopologyCaseV1>,std::string>::Failure(hierarchy.Error());
        result.push_back({id,std::move(hierarchy.Value())});
    }
    return base::Result<std::vector<TopologyCaseV1>,std::string>::Success(std::move(result));
}

base::Result<std::vector<FrameCaseV1>, std::string> BuildFrameCorpusV1(const hierarchy::RigidHierarchySemanticV1& hierarchyValue)
{
    std::vector<FrameCaseV1> result;
    for(std::uint32_t frame=0;frame<12;++frame)
    {
        std::vector<contracts::RotationTranslationV1> inputs;
        inputs.reserve(hierarchyValue.boneCount);
        std::uint64_t state=0x5347453453504952ull ^ (static_cast<std::uint64_t>(frame)<<32) ^ hierarchyValue.boneCount;
        for(std::uint32_t bone=0;bone<hierarchyValue.boneCount;++bone) inputs.push_back(MakeInput(frame,bone,hierarchyValue.boneCount,state));
        auto palette=contracts::BuildDynamicLocalMotorPaletteV1(hierarchyValue.boneCount,inputs);
        if(!palette) return base::Result<std::vector<FrameCaseV1>,std::string>::Failure(palette.Error());
        const std::string id="F"+std::string(frame<10?"0":"")+std::to_string(frame);
        result.push_back({id,std::move(palette.Value())});
    }
    return base::Result<std::vector<FrameCaseV1>,std::string>::Success(std::move(result));
}

base::Result<std::vector<ObservedPointV1>, std::string> BuildIndependentCpuReferenceV1(
    const hierarchy::RigidHierarchySemanticV1& hierarchyValue,
    const contracts::DynamicLocalMotorPaletteV1& palette)
{
    auto hierarchyValid=hierarchy::ValidateRigidHierarchySemanticV1(hierarchyValue);
    if(!hierarchyValid) return base::Result<std::vector<ObservedPointV1>,std::string>::Failure(hierarchyValid.Error());
    auto paletteValid=contracts::ValidateDynamicLocalMotorPaletteV1(palette,hierarchyValue.boneCount);
    if(!paletteValid) return base::Result<std::vector<ObservedPointV1>,std::string>::Failure(paletteValid.Error());
    std::vector<Dq> global(hierarchyValue.boneCount);
    for(const auto bone:hierarchyValue.canonicalBoneOrder)
    {
        Dq local;
        for(std::size_t i=0;i<4;++i){local.r[i]=palette.motors[bone].qr[i];local.d[i]=palette.motors[bone].qd[i];}
        const auto parent=hierarchyValue.parentIndices[bone];
        global[bone]=parent<0?local:Compose(global[static_cast<std::uint32_t>(parent)],local);
    }
    std::vector<ObservedPointV1> result;
    result.reserve(static_cast<std::size_t>(hierarchyValue.boneCount)*CanonicalProbesV1().size());
    for(std::uint32_t bone=0;bone<hierarchyValue.boneCount;++bone)
        for(const auto& probe:CanonicalProbesV1()) result.push_back(Apply(global[bone],probe));
    return base::Result<std::vector<ObservedPointV1>,std::string>::Success(std::move(result));
}

std::vector<std::byte> SerializeReferencePointsV1(const std::vector<ObservedPointV1>& points)
{
    base::BinaryWriter writer;
    for(const auto& p:points){writer.WriteU32(std::bit_cast<std::uint32_t>(p.x==0?0.0f:p.x));writer.WriteU32(std::bit_cast<std::uint32_t>(p.y==0?0.0f:p.y));writer.WriteU32(std::bit_cast<std::uint32_t>(p.z==0?0.0f:p.z));writer.WriteU32(std::bit_cast<std::uint32_t>(p.w));}
    return std::move(writer).Take();
}

base::Result<std::vector<std::byte>, std::string> BuildCorpusFoundationBundleV1()
{
    auto topologies=BuildTopologyCorpusV1();
    if(!topologies) return base::Result<std::vector<std::byte>,std::string>::Failure(topologies.Error());
    base::BinaryWriter writer;
    writer.WriteU32(1); writer.WriteU32(static_cast<std::uint32_t>(topologies.Value().size())); writer.WriteU32(12); writer.WriteU32(5);
    for(const auto& topology:topologies.Value())
    {
        const auto hierarchyBytes=hierarchy::SerializeRigidHierarchySemanticV1(topology.hierarchy);
        writer.WriteU32(static_cast<std::uint32_t>(hierarchyBytes.size())); writer.WriteBytes(hierarchyBytes);
        auto frames=BuildFrameCorpusV1(topology.hierarchy);
        if(!frames) return base::Result<std::vector<std::byte>,std::string>::Failure(frames.Error());
        for(const auto& frame:frames.Value())
        {
            const auto paletteBytes=contracts::SerializeDynamicLocalMotorPaletteV1(frame.palette);
            auto reference=BuildIndependentCpuReferenceV1(topology.hierarchy,frame.palette);
            if(!reference) return base::Result<std::vector<std::byte>,std::string>::Failure(reference.Error());
            const auto referenceBytes=SerializeReferencePointsV1(reference.Value());
            writer.WriteU32(static_cast<std::uint32_t>(paletteBytes.size())); writer.WriteBytes(paletteBytes);
            writer.WriteU32(static_cast<std::uint32_t>(referenceBytes.size())); writer.WriteBytes(referenceBytes);
        }
    }
    return base::Result<std::vector<std::byte>,std::string>::Success(std::move(writer).Take());
}
}
