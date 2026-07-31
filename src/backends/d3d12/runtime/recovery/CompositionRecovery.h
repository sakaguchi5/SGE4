#pragma once
#include "../RuntimeTypes.h"

#include "../composition/CompositionRuntime.h"

#include <cstddef>
#include <cstdint>

namespace sge4::d3d12::runtime_detail
{
struct WholeCompositionRecoveryReport final
{
    ::sge4::runtime::DeviceRecoveryReport device;
    std::size_t leafCountBefore = 0;
    std::size_t leafCountAfter = 0;
    std::size_t resourceCountBefore = 0;
    std::size_t resourceCountAfter = 0;
    bool frozenArtifactRevalidated = false;
    bool allRuntimeObjectsReleased = false;
    bool allRuntimeObjectsRematerialized = false;
};

class WholeCompositionRecovery final
{
public:
    [[nodiscard]] static base::Expected<WholeCompositionRecoveryReport, StaticRuntimeError> Recover(
        LoadedStaticComposition& loaded,
        ::sge4::runtime::DeviceRecoveryMode mode);
};

[[nodiscard]] base::Expected<WholeCompositionRecoveryReport, StaticRuntimeError> RecoverStaticComposition(
    LoadedStaticComposition& loaded,
    ::sge4::runtime::DeviceRecoveryMode mode);
}
