#include "Spiral3Contracts.h"

#include <algorithm>
#include <limits>
#include <span>
#include <string_view>

namespace sge4_5::spiral3::contracts
{
base::Result<FixedReuseWorkloadShapeV1, std::string>
BuildFixedReuseWorkloadShapeV1(BoneCountV1 boneCount, ReuseCountV1 reuseCount)
{
    if (boneCount.Value() == 0)
        return base::Result<FixedReuseWorkloadShapeV1, std::string>::Failure(
            "bone count must be positive");
    if (reuseCount.Value() == 0)
        return base::Result<FixedReuseWorkloadShapeV1, std::string>::Failure(
            "reuse count must be positive");

    const std::uint64_t pointCount =
        static_cast<std::uint64_t>(boneCount.Value()) * reuseCount.Value();
    if (pointCount > MaximumConsumerPointCountV1 ||
        pointCount > std::numeric_limits<std::uint32_t>::max())
    {
        return base::Result<FixedReuseWorkloadShapeV1, std::string>::Failure(
            "consumer point count exceeds the Spiral 3 fixed-count qualification bound");
    }

    FixedReuseWorkloadShapeV1 result{
        boneCount,
        reuseCount,
        PointCountV1{static_cast<std::uint32_t>(pointCount)}};
    return base::Result<FixedReuseWorkloadShapeV1, std::string>::Success(result);
}

bool IsCanonicalReuseCountV1(std::uint32_t value) noexcept
{
    return std::find(CanonicalReuseCountsV1.begin(), CanonicalReuseCountsV1.end(), value) !=
           CanonicalReuseCountsV1.end();
}

base::Digest256 CanonicalReuseSetIdentityV1()
{
    base::BinaryWriter writer;
    constexpr std::string_view domain = "SGE4-5.Spiral3.CanonicalReuseSet.V1";
    writer.WriteBytes(std::as_bytes(std::span(domain.data(), domain.size())));
    for (const auto value : CanonicalReuseCountsV1)
        writer.WriteU32(value);
    return base::Sha256(std::move(writer).Take());
}
}
