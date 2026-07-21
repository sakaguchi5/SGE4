#include "Spiral3Execution.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <limits>

namespace sge4_5::spiral3::execution
{
namespace
{
namespace can=composition::canonical;
namespace runtime=composition::canonical::runtime_v1;
template<class T> base::Result<T,ExecutionErrorV1> Fail(std::string stage,std::string message)
{return base::Result<T,ExecutionErrorV1>::Failure({std::move(stage),std::move(message)});}

bool FinitePoint(const corpus::ObservedPointV1& p)
{return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&std::bit_cast<std::uint32_t>(p.w)==0x3f800000u;}
std::uint32_t OrderedFloatBits(float value)
{
    const auto bits=std::bit_cast<std::uint32_t>(value);
    if((bits&0x7fffffffu)==0u)return 0x80000000u;
    return (bits&0x80000000u)!=0u?~bits:(bits|0x80000000u);
}
std::uint64_t UlpDistance(float a,float b)
{const auto aa=OrderedFloatBits(a),bb=OrderedFloatBits(b);return aa>bb?aa-bb:bb-aa;}
bool MetricNear(float actual,float expected)
{const float tolerance=1.0e-7f+1.0e-6f*std::max(1.0f,std::abs(expected));return std::isfinite(actual)&&std::abs(actual-expected)<=tolerance;}

struct Accumulator final
{
    ErrorMetricSummaryV1 summary;
    long double squaredEuclidean=0;
    void Add(const corpus::ObservedPointV1& actual,const corpus::ObservedPointV1& expected,bool pairwise)
    {
        const std::array<float,3>a{actual.x,actual.y,actual.z},e{expected.x,expected.y,expected.z};
        long double pointSquared=0;
        for(std::size_t i=0;i<3;++i)
        {
            const double absolute=std::abs(static_cast<double>(a[i])-e[i]);
            const double scale=pairwise?std::max({1.0,std::abs(static_cast<double>(a[i])),std::abs(static_cast<double>(e[i]))}):std::max(1.0,std::abs(static_cast<double>(e[i])));
            summary.maxAbsoluteError=std::max(summary.maxAbsoluteError,absolute);
            summary.maxRelativeError=std::max(summary.maxRelativeError,absolute/scale);
            summary.maxUlpDistance=std::max(summary.maxUlpDistance,UlpDistance(a[i],e[i]));
            pointSquared+=static_cast<long double>(absolute)*absolute;
        }
        squaredEuclidean+=pointSquared;
    }
    void Finish(std::size_t count){summary.rmsEuclideanError=count?std::sqrt(static_cast<double>(squaredEuclidean/static_cast<long double>(count))):0;}
};

bool PassReference(const corpus::ObservedPointV1& actual,const corpus::ObservedPointV1& reference)
{
    const std::array<float,3>a{actual.x,actual.y,actual.z},r{reference.x,reference.y,reference.z};
    for(std::size_t i=0;i<3;++i)
    {
        const double error=std::abs(static_cast<double>(a[i])-r[i]);
        const double scale=std::max(1.0,std::abs(static_cast<double>(r[i])));
        if(error>spiral2::contracts::ObservationAbsoluteToleranceV2+spiral2::contracts::ObservationRelativeToleranceV2*scale)return false;
    }
    return true;
}
bool PassPair(const corpus::ObservedPointV1& a,const corpus::ObservedPointV1& b)
{
    const std::array<float,3>av{a.x,a.y,a.z},bv{b.x,b.y,b.z};
    for(std::size_t i=0;i<3;++i)
    {
        const double error=std::abs(static_cast<double>(av[i])-bv[i]);
        const double scale=std::max({1.0,std::abs(static_cast<double>(av[i])),std::abs(static_cast<double>(bv[i]))});
        if(error>spiral2::contracts::ObservationPairwiseAbsoluteToleranceV2+spiral2::contracts::ObservationPairwiseRelativeToleranceV2*scale)return false;
    }
    return true;
}

void WriteMetric(base::BinaryWriter& writer,const ErrorMetricSummaryV1& metric)
{
    writer.WriteU64(std::bit_cast<std::uint64_t>(metric.maxAbsoluteError));
    writer.WriteU64(std::bit_cast<std::uint64_t>(metric.maxRelativeError));
    writer.WriteU64(metric.maxUlpDistance);
    writer.WriteU64(std::bit_cast<std::uint64_t>(metric.rmsEuclideanError));
}

base::Result<std::vector<corpus::ObservedPointV1>,ExecutionErrorV1> DecodePoints(std::span<const std::byte> bytes,std::uint32_t expected)
{
    if(bytes.size()!=static_cast<std::size_t>(expected)*16u)return Fail<std::vector<corpus::ObservedPointV1>>("execution/readback","point byte count is invalid");
    base::BinaryReader reader(bytes);std::vector<corpus::ObservedPointV1> out;out.reserve(expected);
    for(std::uint32_t i=0;i<expected;++i)
    {
        auto x=reader.ReadU32(),y=reader.ReadU32(),z=reader.ReadU32(),w=reader.ReadU32();
        if(!x||!y||!z||!w)return Fail<std::vector<corpus::ObservedPointV1>>("execution/readback","point record is truncated");
        out.push_back({std::bit_cast<float>(x.Value()),std::bit_cast<float>(y.Value()),std::bit_cast<float>(z.Value()),std::bit_cast<float>(w.Value())});
    }
    return base::Result<std::vector<corpus::ObservedPointV1>,ExecutionErrorV1>::Success(std::move(out));
}

base::Result<void,ExecutionErrorV1> ValidateGpuRecords(
    std::span<const std::byte> bytes,
    std::span<const corpus::ObservedPointV1> reference,
    std::span<const corpus::ObservedPointV1> a,
    std::span<const corpus::ObservedPointV1> b,
    std::span<const corpus::ObservedPointV1> c)
{
    constexpr std::uint32_t recordStride=304;
    if(bytes.size()!=reference.size()*recordStride)return Fail<void>("execution/gpu-observer","GPU observation record byte count is invalid");
    base::BinaryReader reader(bytes);
    for(std::size_t i=0;i<reference.size();++i)
    {
        std::array<std::uint32_t,76> words{};
        for(auto& word:words){auto value=reader.ReadU32();if(!value)return Fail<void>("execution/gpu-observer","GPU observation record is truncated");word=value.Value();}
        const std::array<const corpus::ObservedPointV1*,6> left{&a[i],&b[i],&c[i],&a[i],&a[i],&b[i]};
        const std::array<const corpus::ObservedPointV1*,6> right{&reference[i],&reference[i],&reference[i],&b[i],&c[i],&c[i]};
        for(std::size_t relation=0;relation<left.size();++relation)
        {
            const std::array<float,3> lhs{left[relation]->x,left[relation]->y,left[relation]->z},rhs{right[relation]->x,right[relation]->y,right[relation]->z};
            for(std::size_t component=0;component<3;++component)
            {
                const float expectedAbsolute=std::abs(lhs[component]-rhs[component]);
                const float scale=relation<3?std::max(1.0f,std::abs(rhs[component])):std::max({1.0f,std::abs(lhs[component]),std::abs(rhs[component])});
                const float expectedRelative=expectedAbsolute/scale;
                const float gpuAbsolute=std::bit_cast<float>(words[relation*4+component]);
                const float gpuRelative=std::bit_cast<float>(words[24+relation*4+component]);
                if(!MetricNear(gpuAbsolute,expectedAbsolute)||!MetricNear(gpuRelative,expectedRelative)||words[48+relation*4+component]!=UlpDistance(lhs[component],rhs[component]))
                    return Fail<void>("execution/gpu-observer","GPU component metric does not match CPU reconstruction at point "+std::to_string(i));
            }
            if(words[relation*4+3]!=0||words[24+relation*4+3]!=0||words[48+relation*4+3]!=0)
                return Fail<void>("execution/gpu-observer","GPU reserved metric component is nonzero");
        }
        if(words[72]!=1||words[73]!=1||words[74]!=1||words[75]!=i)
            return Fail<void>("execution/gpu-observer","GPU finite, homogeneous, contract-pass or point identity flag failed at point "+std::to_string(i));
    }
    return base::Result<void,ExecutionErrorV1>::Success();
}
}

base::Result<ReuseObservationReportV1,std::string> ObserveReuseComparisonV1(
    std::span<const corpus::ObservedPointV1> r,
    std::span<const corpus::ObservedPointV1> a,
    std::span<const corpus::ObservedPointV1> b,
    std::span<const corpus::ObservedPointV1> c,
    std::uint32_t boneCount,
    std::uint32_t reuseCount)
{
    const std::uint64_t expected=static_cast<std::uint64_t>(boneCount)*reuseCount;
    if(boneCount==0||reuseCount==0||r.size()!=expected||a.size()!=r.size()||b.size()!=r.size()||c.size()!=r.size())
        return base::Result<ReuseObservationReportV1,std::string>::Failure("reuse observation shapes differ");
    ReuseObservationReportV1 out;out.pointCount=r.size();Accumulator ar,br,cr,ab,ac,bc;
    for(std::size_t i=0;i<r.size();++i)
    {
        if(!FinitePoint(r[i])||!FinitePoint(a[i])||!FinitePoint(b[i])||!FinitePoint(c[i]))
        {++out.nonFiniteCount;++out.mismatchCount;if(out.firstMismatchIndex==std::numeric_limits<std::uint64_t>::max())out.firstMismatchIndex=i;continue;}
        ar.Add(a[i],r[i],false);br.Add(b[i],r[i],false);cr.Add(c[i],r[i],false);ab.Add(a[i],b[i],true);ac.Add(a[i],c[i],true);bc.Add(b[i],c[i],true);
        if(!PassReference(a[i],r[i])||!PassReference(b[i],r[i])||!PassReference(c[i],r[i])||!PassPair(a[i],b[i])||!PassPair(a[i],c[i])||!PassPair(b[i],c[i]))
        {++out.mismatchCount;if(out.firstMismatchIndex==std::numeric_limits<std::uint64_t>::max())out.firstMismatchIndex=i;}
    }
    ar.Finish(r.size());br.Finish(r.size());cr.Finish(r.size());ab.Finish(r.size());ac.Finish(r.size());bc.Finish(r.size());
    out.aReference=ar.summary;out.bReference=br.summary;out.cReference=cr.summary;out.aB=ab.summary;out.aC=ac.summary;out.bC=bc.summary;
    out.maxAbsoluteError=std::max({ar.summary.maxAbsoluteError,br.summary.maxAbsoluteError,cr.summary.maxAbsoluteError});
    out.maxRelativeError=std::max({ar.summary.maxRelativeError,br.summary.maxRelativeError,cr.summary.maxRelativeError});
    out.maxUlpDistance=std::max({ar.summary.maxUlpDistance,br.summary.maxUlpDistance,cr.summary.maxUlpDistance});
    out.maxPairwiseError=std::max({ab.summary.maxAbsoluteError,ac.summary.maxAbsoluteError,bc.summary.maxAbsoluteError});
    out.maxPairwiseRelativeError=std::max({ab.summary.maxRelativeError,ac.summary.maxRelativeError,bc.summary.maxRelativeError});
    out.maxPairwiseUlpDistance=std::max({ab.summary.maxUlpDistance,ac.summary.maxUlpDistance,bc.summary.maxUlpDistance});

    if(reuseCount>=4)
    {
        out.rigidityQualified=true;out.minimumDeterminant=std::numeric_limits<double>::infinity();bool rigidityFailed=false;std::uint64_t firstRigidity=std::numeric_limits<std::uint64_t>::max();
        for(std::uint32_t bone=0;bone<boneCount;++bone)
        {
            const std::size_t origin=static_cast<std::size_t>(bone)*reuseCount;
            for(const auto* values:{&a,&b,&c})
            {
                const auto& o=(*values)[origin];const auto& ro=r[origin];
                out.maxTranslationError=std::max({out.maxTranslationError,std::abs(static_cast<double>(o.x)-ro.x),std::abs(static_cast<double>(o.y)-ro.y),std::abs(static_cast<double>(o.z)-ro.z)});
                double axis[3][3]{};
                for(std::size_t j=0;j<3;++j)
                {
                    const auto& p=(*values)[origin+j+1];
                    axis[j][0]=p.x-o.x;axis[j][1]=p.y-o.y;axis[j][2]=p.z-o.z;
                    const double length=std::sqrt(axis[j][0]*axis[j][0]+axis[j][1]*axis[j][1]+axis[j][2]*axis[j][2]);
                    out.maxAxisLengthError=std::max(out.maxAxisLengthError,std::abs(length-1.0));
                }
                for(int x=0;x<3;++x)for(int y=x+1;y<3;++y)
                    out.maxOrthogonalityError=std::max(out.maxOrthogonalityError,std::abs(axis[x][0]*axis[y][0]+axis[x][1]*axis[y][1]+axis[x][2]*axis[y][2]));
                const double determinant=axis[0][0]*(axis[1][1]*axis[2][2]-axis[1][2]*axis[2][1])-axis[0][1]*(axis[1][0]*axis[2][2]-axis[1][2]*axis[2][0])+axis[0][2]*(axis[1][0]*axis[2][1]-axis[1][1]*axis[2][0]);
                out.minimumDeterminant=std::min(out.minimumDeterminant,determinant);if(!(determinant>0))out.orientationPreserved=false;
            }
            if(out.maxAxisLengthError>spiral2::contracts::ObservationRigidityToleranceV2||out.maxOrthogonalityError>spiral2::contracts::ObservationRigidityToleranceV2||!out.orientationPreserved)
            {rigidityFailed=true;if(firstRigidity==std::numeric_limits<std::uint64_t>::max())firstRigidity=origin;}
        }
        if(rigidityFailed){++out.mismatchCount;if(out.firstMismatchIndex==std::numeric_limits<std::uint64_t>::max())out.firstMismatchIndex=firstRigidity;}
        if(!std::isfinite(out.minimumDeterminant))out.minimumDeterminant=0;
    }
    return base::Result<ReuseObservationReportV1,std::string>::Success(out);
}

base::Result<ReuseComparisonExecutionResultV1,ExecutionErrorV1> ExecuteLoadedReuseComparisonScenarioV1(
    const scenarios::FrozenReuseComparisonScenarioV1& scenario,
    const spiral2::hierarchy::RigidHierarchySemanticV1& hierarchy,
    const corpus::ReuseCaseV1& reuseCase,
    const spiral2::contracts::DynamicLocalMotorPaletteV1& palette,
    runtime::LoadedStaticComposition& loaded,
    std::uint64_t frameNumber)
{
    auto scenarioValid=scenarios::ValidateFrozenReuseComparisonScenarioV1(hierarchy,reuseCase,scenario);if(!scenarioValid)return Fail<ReuseComparisonExecutionResultV1>(scenarioValid.Error().stage,scenarioValid.Error().message);
    auto paletteValid=spiral2::contracts::ValidateDynamicLocalMotorPaletteV1(palette,hierarchy.boneCount);if(!paletteValid)return Fail<ReuseComparisonExecutionResultV1>("execution/palette",paletteValid.Error());
    if(base::Sha256(loaded.Artifact().Artifact().FileBytes())!=scenario.frozenCompositionDigest)return Fail<ReuseComparisonExecutionResultV1>("execution/frozen","loaded Composition digest does not match the scenario");
    auto reference=corpus::BuildIndependentCpuReferenceV1(hierarchy,palette,reuseCase.localPoints,contracts::ReuseCountV1{reuseCase.reuseCount});if(!reference)return Fail<ReuseComparisonExecutionResultV1>("execution/reference",reference.Error());
    const auto motorBytes=spiral2::contracts::SerializeDynamicLocalMotorPaletteV1(palette);const auto referenceBytes=corpus::SerializePointsV1(reference.Value());
    if(scenario.leaves.empty()||scenario.leaves[0].expectedDynamicSlotCount!=2||scenario.leaves[0].lockedDynamicBytes!=motorBytes.size()||scenario.leaves[0].lockedReferenceBytes!=referenceBytes.size())return Fail<ReuseComparisonExecutionResultV1>("execution/dynamic-authority","dynamic source slot shape does not match frozen Leaf authority");
    const auto& contract=loaded.Artifact().ValidatedContract().Contract();
    const auto source=scenarios::FindLeafByAuthorKeyV1(contract,scenario.leaves[0].role);
    const auto referenceFlow=scenarios::FindFlowByAuthorKeyV1(contract,scenarios::ReferenceFlowKeyV1),pointsFlow=scenarios::FindFlowByAuthorKeyV1(contract,scenarios::PointsFlowKeyV1),aFlow=scenarios::FindFlowByAuthorKeyV1(contract,scenarios::AOutputFlowKeyV1),bFlow=scenarios::FindFlowByAuthorKeyV1(contract,scenarios::BOutputFlowKeyV1),cFlow=scenarios::FindFlowByAuthorKeyV1(contract,scenarios::COutputFlowKeyV1),recordsFlow=scenarios::FindFlowByAuthorKeyV1(contract,scenarios::RecordsFlowKeyV1);
    if(!source.IsValid()||!referenceFlow.IsValid()||!pointsFlow.IsValid()||!aFlow.IsValid()||!bFlow.IsValid()||!cFlow.IsValid()||!recordsFlow.IsValid())return Fail<ReuseComparisonExecutionResultV1>("execution/identity","required canonical Leaf or Flow identity was not found");
    runtime::StaticCompositionFrameInvocation invocation;invocation.frameNumber=frameNumber;invocation.dynamicData={{source,0u,motorBytes},{source,1u,referenceBytes}};
    auto submitted=runtime::SubmitStaticComposition(loaded,invocation);if(!submitted)return Fail<ReuseComparisonExecutionResultV1>(submitted.Error().stage,submitted.Error().message);
    if(submitted.Value().executionOrder.size()!=11||submitted.Value().leaves.size()!=11)return Fail<ReuseComparisonExecutionResultV1>("execution/runtime","Frozen Composition did not execute exactly eleven Leaves");
    auto referenceRead=runtime::ReadStaticCompositionBuffer(loaded,referenceFlow),pointsRead=runtime::ReadStaticCompositionBuffer(loaded,pointsFlow),aRead=runtime::ReadStaticCompositionBuffer(loaded,aFlow),bRead=runtime::ReadStaticCompositionBuffer(loaded,bFlow),cRead=runtime::ReadStaticCompositionBuffer(loaded,cFlow),recordsRead=runtime::ReadStaticCompositionBuffer(loaded,recordsFlow);
    if(!referenceRead||!pointsRead||!aRead||!bRead||!cRead||!recordsRead)
    {const auto* error=!referenceRead?&referenceRead.Error():!pointsRead?&pointsRead.Error():!aRead?&aRead.Error():!bRead?&bRead.Error():!cRead?&cRead.Error():&recordsRead.Error();return Fail<ReuseComparisonExecutionResultV1>(error->stage,error->message);}
    if(referenceRead.Value().bytes!=referenceBytes)return Fail<ReuseComparisonExecutionResultV1>("execution/reference-source","dynamic source did not reproduce independent reference bytes");
    if(pointsRead.Value().bytes!=corpus::SerializePointsV1(reuseCase.localPoints))return Fail<ReuseComparisonExecutionResultV1>("execution/point-source","Package-owned point source did not reproduce the frozen point corpus");
    auto a=DecodePoints(aRead.Value().bytes,scenario.pointCount),b=DecodePoints(bRead.Value().bytes,scenario.pointCount),c=DecodePoints(cRead.Value().bytes,scenario.pointCount);if(!a)return base::Result<ReuseComparisonExecutionResultV1,ExecutionErrorV1>::Failure(a.Error());if(!b)return base::Result<ReuseComparisonExecutionResultV1,ExecutionErrorV1>::Failure(b.Error());if(!c)return base::Result<ReuseComparisonExecutionResultV1,ExecutionErrorV1>::Failure(c.Error());
    auto observed=ObserveReuseComparisonV1(reference.Value(),a.Value(),b.Value(),c.Value(),hierarchy.boneCount,reuseCase.reuseCount);if(!observed)return Fail<ReuseComparisonExecutionResultV1>("execution/observer",observed.Error());
    if(observed.Value().mismatchCount||observed.Value().nonFiniteCount)return Fail<ReuseComparisonExecutionResultV1>("execution/observer","WARP outputs violate the reference or pairwise contract: max-reference="+std::to_string(observed.Value().maxAbsoluteError)+", max-pairwise="+std::to_string(observed.Value().maxPairwiseError));
    auto gpuValid=ValidateGpuRecords(recordsRead.Value().bytes,reference.Value(),a.Value(),b.Value(),c.Value());if(!gpuValid)return base::Result<ReuseComparisonExecutionResultV1,ExecutionErrorV1>::Failure(gpuValid.Error());
    ReuseComparisonExecutionResultV1 out;out.reference=std::move(reference.Value());out.candidateA=std::move(a.Value());out.candidateB=std::move(b.Value());out.candidateC=std::move(c.Value());out.observation=observed.Value();out.gpuRecords=std::move(recordsRead.Value().bytes);out.deviceEpoch=submitted.Value().deviceEpoch;return base::Result<ReuseComparisonExecutionResultV1,ExecutionErrorV1>::Success(std::move(out));
}

base::Result<ReuseComparisonExecutionResultV1,ExecutionErrorV1> ExecuteFrozenReuseComparisonScenarioV1(
    const scenarios::FrozenReuseComparisonScenarioV1& scenario,
    const spiral2::hierarchy::RigidHierarchySemanticV1& hierarchy,
    const corpus::ReuseCaseV1& reuseCase,
    const spiral2::contracts::DynamicLocalMotorPaletteV1& palette,
    d3d12::D3D12Backend& backend,
    std::uint64_t frameNumber)
{
    auto loaded=runtime::LoadStaticComposition(scenario.frozenCompositionBytes,backend);if(!loaded)return Fail<ReuseComparisonExecutionResultV1>(loaded.Error().stage,loaded.Error().message);return ExecuteLoadedReuseComparisonScenarioV1(scenario,hierarchy,reuseCase,palette,loaded.Value(),frameNumber);
}

base::Result<ReuseCorpusExecutionResultV1,ExecutionErrorV1> ExecuteFrozenReuseCorpusScenarioV1(
    const scenarios::FrozenReuseComparisonScenarioV1& scenario,
    const spiral2::hierarchy::RigidHierarchySemanticV1& hierarchy,
    const corpus::ReuseCaseV1& reuseCase,
    std::span<const spiral2::corpus::FrameCaseV1> frames,
    d3d12::D3D12Backend& backend,
    std::uint64_t firstFrameNumber)
{
    if(frames.empty())
        return Fail<ReuseCorpusExecutionResultV1>("execution/corpus","frame corpus is empty");
    auto loaded=runtime::LoadStaticComposition(scenario.frozenCompositionBytes,backend);
    if(!loaded)
        return Fail<ReuseCorpusExecutionResultV1>(loaded.Error().stage,loaded.Error().message);
    ReuseCorpusExecutionResultV1 out;out.frozenDigestBefore=base::Sha256(loaded.Value().Artifact().Artifact().FileBytes());out.frames.reserve(frames.size());std::uint64_t epoch=0;
    for(std::size_t i=0;i<frames.size();++i)
    {
        auto executed=ExecuteLoadedReuseComparisonScenarioV1(scenario,hierarchy,reuseCase,frames[i].palette,loaded.Value(),firstFrameNumber+i);if(!executed)return Fail<ReuseCorpusExecutionResultV1>(executed.Error().stage+"/"+frames[i].id,executed.Error().message);if(i==0)epoch=executed.Value().deviceEpoch;else if(executed.Value().deviceEpoch!=epoch)return Fail<ReuseCorpusExecutionResultV1>("execution/corpus","device epoch changed during the twelve-frame corpus");out.frames.push_back(std::move(executed.Value()));
    }
    out.frozenDigestAfter=base::Sha256(loaded.Value().Artifact().Artifact().FileBytes());if(out.frozenDigestBefore!=out.frozenDigestAfter||out.frozenDigestBefore!=scenario.frozenCompositionDigest)return Fail<ReuseCorpusExecutionResultV1>("execution/corpus","Frozen Composition changed across dynamic frames");return base::Result<ReuseCorpusExecutionResultV1,ExecutionErrorV1>::Success(std::move(out));
}

std::vector<std::byte> SerializeReuseObservationReportV1(const ReuseObservationReportV1& report)
{
    base::BinaryWriter writer;writer.WriteU64(report.pointCount);writer.WriteU64(report.mismatchCount);writer.WriteU64(report.nonFiniteCount);writer.WriteU64(report.firstMismatchIndex);WriteMetric(writer,report.aReference);WriteMetric(writer,report.bReference);WriteMetric(writer,report.cReference);WriteMetric(writer,report.aB);WriteMetric(writer,report.aC);WriteMetric(writer,report.bC);writer.WriteU64(std::bit_cast<std::uint64_t>(report.maxAbsoluteError));writer.WriteU64(std::bit_cast<std::uint64_t>(report.maxRelativeError));writer.WriteU64(report.maxUlpDistance);writer.WriteU64(std::bit_cast<std::uint64_t>(report.maxPairwiseError));writer.WriteU64(std::bit_cast<std::uint64_t>(report.maxPairwiseRelativeError));writer.WriteU64(report.maxPairwiseUlpDistance);writer.WriteU32(report.rigidityQualified?1u:0u);writer.WriteU32(report.orientationPreserved?1u:0u);writer.WriteU64(std::bit_cast<std::uint64_t>(report.maxAxisLengthError));writer.WriteU64(std::bit_cast<std::uint64_t>(report.maxOrthogonalityError));writer.WriteU64(std::bit_cast<std::uint64_t>(report.maxTranslationError));writer.WriteU64(std::bit_cast<std::uint64_t>(report.minimumDeterminant));return std::move(writer).Take();
}
std::vector<std::byte> SerializeReuseExecutionEvidenceV1(const scenarios::FrozenReuseComparisonScenarioV1& scenario,const ReuseComparisonExecutionResultV1& result)
{
    base::BinaryWriter writer;constexpr std::string_view domain="SGE4-5.Spiral3.CU5.ExecutionEvidence.V1";writer.WriteU32(static_cast<std::uint32_t>(domain.size()));writer.WriteBytes(std::as_bytes(std::span(domain.data(),domain.size())));writer.WriteBytes(scenario.scenarioIdentity);writer.WriteBytes(scenario.frozenCompositionDigest);writer.WriteU32(scenario.reuseCount);writer.WriteU32(scenario.pointCount);for(const auto* points:{&result.reference,&result.candidateA,&result.candidateB,&result.candidateC}){auto bytes=corpus::SerializePointsV1(*points);writer.WriteU64(bytes.size());writer.WriteBytes(bytes);}auto observation=SerializeReuseObservationReportV1(result.observation);writer.WriteU64(observation.size());writer.WriteBytes(observation);writer.WriteU64(result.gpuRecords.size());writer.WriteBytes(result.gpuRecords);return std::move(writer).Take();
}
}
