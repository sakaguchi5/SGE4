#include "Spiral2Observer.h"
#include <algorithm>
#include <bit>
#include <cmath>

namespace sge4_5::spiral2::observer
{
base::Result<ObservationReportV1,std::string> ObserveHierarchyV1(std::span<const corpus::ObservedPointV1> r,std::span<const corpus::ObservedPointV1> a,std::span<const corpus::ObservedPointV1> b,std::span<const corpus::ObservedPointV1> c)
{
    if(r.empty()||r.size()!=a.size()||r.size()!=b.size()||r.size()!=c.size()||r.size()%5!=0)return base::Result<ObservationReportV1,std::string>::Failure("observation shapes differ");ObservationReportV1 out;out.pointCount=r.size();
    auto finite=[](const auto&p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&p.w==1.0f;};auto err=[](const auto&x,const auto&y){return std::max({std::abs(static_cast<double>(x.x)-y.x),std::abs(static_cast<double>(x.y)-y.y),std::abs(static_cast<double>(x.z)-y.z)});};
    for(std::size_t i=0;i<r.size();++i){if(!finite(r[i])||!finite(a[i])||!finite(b[i])||!finite(c[i])){++out.nonFiniteCount;++out.mismatchCount;continue;}const double ea=err(a[i],r[i]),eb=err(b[i],r[i]),ec=err(c[i],r[i]);out.maxAbsoluteError=std::max({out.maxAbsoluteError,ea,eb,ec});out.maxPairwiseError=std::max({out.maxPairwiseError,err(a[i],b[i]),err(a[i],c[i]),err(b[i],c[i])});const double scale=std::max({1.0,std::abs(static_cast<double>(r[i].x)),std::abs(static_cast<double>(r[i].y)),std::abs(static_cast<double>(r[i].z))});if(std::max({ea,eb,ec})>2.5e-5+1.0e-5*scale||out.maxPairwiseError>5.0e-5+1.0e-5*scale)++out.mismatchCount;}
    for(std::size_t bone=0;bone<r.size()/5;++bone){const auto o=bone*5;for(const auto* values:{&a,&b,&c}){double axis[3][3];for(int j=0;j<3;++j){const auto&p=(*values)[o+j+1];const auto&q=(*values)[o];axis[j][0]=p.x-q.x;axis[j][1]=p.y-q.y;axis[j][2]=p.z-q.z;const double length=std::sqrt(axis[j][0]*axis[j][0]+axis[j][1]*axis[j][1]+axis[j][2]*axis[j][2]);out.maxAxisLengthError=std::max(out.maxAxisLengthError,std::abs(length-1.0));}for(int x=0;x<3;++x)for(int y=x+1;y<3;++y)out.maxOrthogonalityError=std::max(out.maxOrthogonalityError,std::abs(axis[x][0]*axis[y][0]+axis[x][1]*axis[y][1]+axis[x][2]*axis[y][2]));const double det=axis[0][0]*(axis[1][1]*axis[2][2]-axis[1][2]*axis[2][1])-axis[0][1]*(axis[1][0]*axis[2][2]-axis[1][2]*axis[2][0])+axis[0][2]*(axis[1][0]*axis[2][1]-axis[1][1]*axis[2][0]);if(!(det>0))out.orientationPreserved=false;}}
    if(out.maxAxisLengthError>5e-4||out.maxOrthogonalityError>5e-4||!out.orientationPreserved)++out.mismatchCount;return base::Result<ObservationReportV1,std::string>::Success(out);
}
std::vector<std::byte> SerializeObservationReportV1(const ObservationReportV1& r){base::BinaryWriter w;w.WriteU64(r.pointCount);w.WriteU64(r.mismatchCount);w.WriteU64(r.nonFiniteCount);w.WriteU64(std::bit_cast<std::uint64_t>(r.maxAbsoluteError));w.WriteU64(std::bit_cast<std::uint64_t>(r.maxPairwiseError));w.WriteU64(std::bit_cast<std::uint64_t>(r.maxAxisLengthError));w.WriteU64(std::bit_cast<std::uint64_t>(r.maxOrthogonalityError));w.WriteU32(r.orientationPreserved?1:0);w.WriteU32(0);return std::move(w).Take();}
}
