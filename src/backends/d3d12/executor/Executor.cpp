#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "./Executor.h"

#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

// Windows SDK compatibility: keep legacy min/max macros from rewriting
// qualified C++ calls such as std::max(...).
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <climits>
#include <cstring>
#include <iomanip>
#include <memory>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")

namespace sge4::d3d12
{
using Microsoft::WRL::ComPtr;
namespace pkg = package::d3d12_v13;

namespace detail
{
struct TimestampProfileRecord final
{
    base::Digest256 packageExecutionDigest{};
    std::uint64_t frameNumber = 0;
    std::uint64_t instanceOrdinal = 0;
    std::uint64_t submissionOrdinal = 0;
    double commandRecordingNanoseconds = 0.0;
    std::uint64_t timestampFrequency = 0;
    std::uint64_t* mappedQueryValues = nullptr;
    std::uint32_t dispatchCount = 0;
    std::uint32_t barrierCount = 0;
    bool ready = false;
};

struct TimestampProfileCollector final
{
    std::mutex mutex;
    std::vector<std::weak_ptr<TimestampProfileRecord>> records;
    std::uint64_t nextInstanceOrdinal = 0;
    std::uint64_t nextSubmissionOrdinal = 0;
};
}

namespace
{
#include "./detail/ExecutorDiagnostics.inl"
#include "./detail/ExecutorDeviceDomain.inl"
#include "./detail/ExecutorExternalResources.inl"
#include "./detail/ExecutorInstance.inl"
}

#include "./detail/ExecutorApi.inl"
}
