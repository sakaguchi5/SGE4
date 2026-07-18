#pragma once

#include "../22_CompositionRuntime/CompositionRuntime.h"

#include <cstddef>
#include <cstdint>

namespace sge4::composition::canonical::runtime_v1
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
    [[nodiscard]] static base::Result<WholeCompositionRecoveryReport, StaticRuntimeError> Recover(
        LoadedStaticComposition& loaded,
        ::sge4::runtime::DeviceRecoveryMode mode);
};

[[nodiscard]] base::Result<WholeCompositionRecoveryReport, StaticRuntimeError> RecoverStaticComposition(
    LoadedStaticComposition& loaded,
    ::sge4::runtime::DeviceRecoveryMode mode);
}
