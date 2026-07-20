#include "Spiral2Observer.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <limits>

namespace sge4_5::spiral2::observer
{
namespace
{
bool Finite(const corpus::ObservedPointV1& p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&std::bit_cast<std::uint32_t>(p.w)==0x3f800000u;}
std::uint32_t OrderedFloatBits(float value)
{
    const auto bits=std::bit_cast<std::uint32_t>(value);
    if((bits&0x7fffffffu)==0u) return 0x80000000u;
    return (bits&0x80000000u)!=0?~bits:(bits|0x80000000u);
}
std::uint64_t Ulp(float a,float b)
{
    const auto aa=OrderedFloatBits(a),bb=OrderedFloatBits(b);return aa>bb?static_cast<std::uint64_t>(aa-bb):static_cast<std::uint64_t>(bb-aa);
}
struct Accumulator
{
    ErrorMetricSummaryV2 summary;
    long double squaredEuclidean=0;
    void Add(const corpus::ObservedPointV1& actual,const corpus::ObservedPointV1& expected,bool pairwise)
    {
        const std::array<float,3> a{actual.x,actual.y,actual.z},e{expected.x,expected.y,expected.z};long double pointSquared=0;
        for(std::size_t i=0;i<3;++i){const double absolute=std::abs(static_cast<double>(a[i])-e[i]);const double scale=pairwise?std::max({1.0,std::abs(static_cast<double>(a[i])),std::abs(static_cast<double>(e[i]))}):std::max(1.0,std::abs(static_cast<double>(e[i])));summary.maxAbsoluteError=std::max(summary.maxAbsoluteError,absolute);summary.maxRelativeError=std::max(summary.maxRelativeError,absolute/scale);summary.maxUlpDistance=std::max(summary.maxUlpDistance,Ulp(a[i],e[i]));pointSquared+=static_cast<long double>(absolute)*absolute;}
        squaredEuclidean+=pointSquared;
    }
    void Finish(std::size_t count){summary.rmsEuclideanError=count?std::sqrt(static_cast<double>(squaredEuclidean/static_cast<long double>(count))):0;}
};
bool PassReference(const corpus::ObservedPointV1& actual,const corpus::ObservedPointV1& reference)
{
    const std::array<float,3> a{actual.x,actual.y,actual.z},r{reference.x,reference.y,reference.z};for(std::size_t i=0;i<3;++i){const double error=std::abs(static_cast<double>(a[i])-r[i]);const double scale=std::max(1.0,std::abs(static_cast<double>(r[i])));if(error>contracts::ObservationAbsoluteToleranceV2+contracts::ObservationRelativeToleranceV2*scale)return false;}return true;
}
bool PassPair(const corpus::ObservedPointV1& a,const corpus::ObservedPointV1& b)
{
    const std::array<float,3> av{a.x,a.y,a.z},bv{b.x,b.y,b.z};for(std::size_t i=0;i<3;++i){const double error=std::abs(static_cast<double>(av[i])-bv[i]);const double scale=std::max({1.0,std::abs(static_cast<double>(av[i])),std::abs(static_cast<double>(bv[i]))});if(error>contracts::ObservationPairwiseAbsoluteToleranceV2+contracts::ObservationPairwiseRelativeToleranceV2*scale)return false;}return true;
}
void WriteMetric(base::BinaryWriter& w,const ErrorMetricSummaryV2& m){w.WriteU64(std::bit_cast<std::uint64_t>(m.maxAbsoluteError));w.WriteU64(std::bit_cast<std::uint64_t>(m.maxRelativeError));w.WriteU64(m.maxUlpDistance);w.WriteU64(std::bit_cast<std::uint64_t>(m.rmsEuclideanError));}
}
base::Result<ObservationReportV1,std::string> ObserveHierarchyV1(std::span<const corpus::ObservedPointV1> r,std::span<const corpus::ObservedPointV1> a,std::span<const corpus::ObservedPointV1> b,std::span<const corpus::ObservedPointV1> c)
{
    if(r.empty()||r.size()!=a.size()||r.size()!=b.size()||r.size()!=c.size()||r.size()%5!=0)return base::Result<ObservationReportV1,std::string>::Failure("observation shapes differ");ObservationReportV1 out;out.pointCount=r.size();out.minimumDeterminant=std::numeric_limits<double>::infinity();Accumulator ar,br,cr,ab,ac,bc;
    for(std::size_t i=0;i<r.size();++i)
    {
        if(!Finite(r[i])||!Finite(a[i])||!Finite(b[i])||!Finite(c[i])){++out.nonFiniteCount;++out.mismatchCount;if(out.firstMismatchIndex==std::numeric_limits<std::uint64_t>::max())out.firstMismatchIndex=i;continue;}
        ar.Add(a[i],r[i],false);br.Add(b[i],r[i],false);cr.Add(c[i],r[i],false);ab.Add(a[i],b[i],true);ac.Add(a[i],c[i],true);bc.Add(b[i],c[i],true);
        if(!PassReference(a[i],r[i])||!PassReference(b[i],r[i])||!PassReference(c[i],r[i])||!PassPair(a[i],b[i])||!PassPair(a[i],c[i])||!PassPair(b[i],c[i])){++out.mismatchCount;if(out.firstMismatchIndex==std::numeric_limits<std::uint64_t>::max())out.firstMismatchIndex=i;}
    }
    ar.Finish(r.size());br.Finish(r.size());cr.Finish(r.size());ab.Finish(r.size());ac.Finish(r.size());bc.Finish(r.size());out.matrixReference=ar.summary;out.directReference=br.summary;out.hybridReference=cr.summary;out.matrixDirect=ab.summary;out.matrixHybrid=ac.summary;out.directHybrid=bc.summary;
    out.maxAbsoluteError=std::max({ar.summary.maxAbsoluteError,br.summary.maxAbsoluteError,cr.summary.maxAbsoluteError});out.maxRelativeError=std::max({ar.summary.maxRelativeError,br.summary.maxRelativeError,cr.summary.maxRelativeError});out.maxUlpDistance=std::max({ar.summary.maxUlpDistance,br.summary.maxUlpDistance,cr.summary.maxUlpDistance});out.maxPairwiseError=std::max({ab.summary.maxAbsoluteError,ac.summary.maxAbsoluteError,bc.summary.maxAbsoluteError});out.maxPairwiseRelativeError=std::max({ab.summary.maxRelativeError,ac.summary.maxRelativeError,bc.summary.maxRelativeError});out.maxPairwiseUlpDistance=std::max({ab.summary.maxUlpDistance,ac.summary.maxUlpDistance,bc.summary.maxUlpDistance});
    for(std::size_t bone=0;bone<r.size()/5;++bone)
    {
        const auto o=bone*5;for(const auto* values:{&a,&b,&c})
        {
            const auto translation=std::max({std::abs(static_cast<double>((*values)[o].x)-r[o].x),std::abs(static_cast<double>((*values)[o].y)-r[o].y),std::abs(static_cast<double>((*values)[o].z)-r[o].z)});out.maxTranslationError=std::max(out.maxTranslationError,translation);double axis[3][3];for(int j=0;j<3;++j){const auto&p=(*values)[o+j+1];const auto&q=(*values)[o];axis[j][0]=p.x-q.x;axis[j][1]=p.y-q.y;axis[j][2]=p.z-q.z;const double length=std::sqrt(axis[j][0]*axis[j][0]+axis[j][1]*axis[j][1]+axis[j][2]*axis[j][2]);out.maxAxisLengthError=std::max(out.maxAxisLengthError,std::abs(length-1.0));}for(int x=0;x<3;++x)for(int y=x+1;y<3;++y)out.maxOrthogonalityError=std::max(out.maxOrthogonalityError,std::abs(axis[x][0]*axis[y][0]+axis[x][1]*axis[y][1]+axis[x][2]*axis[y][2]));const double det=axis[0][0]*(axis[1][1]*axis[2][2]-axis[1][2]*axis[2][1])-axis[0][1]*(axis[1][0]*axis[2][2]-axis[1][2]*axis[2][0])+axis[0][2]*(axis[1][0]*axis[2][1]-axis[1][1]*axis[2][0]);out.minimumDeterminant=std::min(out.minimumDeterminant,det);if(!(det>0))out.orientationPreserved=false;
        }
        if(out.maxAxisLengthError>contracts::ObservationRigidityToleranceV2||out.maxOrthogonalityError>contracts::ObservationRigidityToleranceV2||!out.orientationPreserved){if(out.firstMismatchIndex==std::numeric_limits<std::uint64_t>::max())out.firstMismatchIndex=o;}
    }
    if(out.maxAxisLengthError>contracts::ObservationRigidityToleranceV2||out.maxOrthogonalityError>contracts::ObservationRigidityToleranceV2||!out.orientationPreserved)++out.mismatchCount;if(!std::isfinite(out.minimumDeterminant))out.minimumDeterminant=0;return base::Result<ObservationReportV1,std::string>::Success(out);
}
std::vector<std::byte> SerializeObservationReportV1(const ObservationReportV1& r)
{
    base::BinaryWriter w;w.WriteU64(r.pointCount);w.WriteU64(r.mismatchCount);w.WriteU64(r.nonFiniteCount);w.WriteU64(r.firstMismatchIndex);WriteMetric(w,r.matrixReference);WriteMetric(w,r.directReference);WriteMetric(w,r.hybridReference);WriteMetric(w,r.matrixDirect);WriteMetric(w,r.matrixHybrid);WriteMetric(w,r.directHybrid);w.WriteU64(std::bit_cast<std::uint64_t>(r.maxAbsoluteError));w.WriteU64(std::bit_cast<std::uint64_t>(r.maxRelativeError));w.WriteU64(r.maxUlpDistance);w.WriteU64(std::bit_cast<std::uint64_t>(r.maxPairwiseError));w.WriteU64(std::bit_cast<std::uint64_t>(r.maxPairwiseRelativeError));w.WriteU64(r.maxPairwiseUlpDistance);w.WriteU64(std::bit_cast<std::uint64_t>(r.maxAxisLengthError));w.WriteU64(std::bit_cast<std::uint64_t>(r.maxOrthogonalityError));w.WriteU64(std::bit_cast<std::uint64_t>(r.maxTranslationError));w.WriteU64(std::bit_cast<std::uint64_t>(r.minimumDeterminant));w.WriteU32(r.orientationPreserved?1:0);w.WriteU32(0);return std::move(w).Take();
}
}
