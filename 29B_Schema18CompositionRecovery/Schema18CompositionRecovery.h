#pragma once

#include "../00_Foundation/Result.h"
#include "../13_PackageRuntime/PackageRuntime.h"
#include "../29A_Schema18StaticDagRuntime/Schema18StaticDagRuntime.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace sge4::composition::schema18::runtime_v1
{
struct CompositionRecoveryError final
{
    std::string stage;
    std::string message;
};

struct CompositionRecoveryReport final
{
    runtime::DeviceRecoveryReport device;
    std::size_t leafCountAfter = 0;
    std::size_t resourceCountAfter = 0;
    bool endpointTokensReset = false;
    bool resourcesRematerialized = false;
};

// Internal friend bridge: F10 is the only layer allowed to replace the private
// F9 materialization while preserving the Frozen Artifact and static schedule.
class StaticCompositionRecoveryAccess final
{
public:
    [[nodiscard]] static base::Result<CompositionRecoveryReport, CompositionRecoveryError>
    Recover(LoadedStaticComposition& loaded, runtime::DeviceRecoveryMode mode);
};

[[nodiscard]] base::Result<CompositionRecoveryReport, CompositionRecoveryError>
RecoverStaticComposition(
    LoadedStaticComposition& loaded,
    runtime::DeviceRecoveryMode mode);
}
