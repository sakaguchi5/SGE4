#include "./PackageRuntime.h"

namespace sge4::runtime
{
base::Expected<LoadedPackage, RuntimeError> LoadPackage(
    package::FrozenExecutablePackage package,
    IPackageExecutor& executor,
    ISurfaceHost* surface)
{
    auto sharedPackage = std::make_shared<const package::FrozenExecutablePackage>(std::move(package));
    auto instance = executor.Load(sharedPackage, surface);
    if (!instance) return base::Failure<LoadedPackage, RuntimeError>(instance.error());
    return base::Success<LoadedPackage, RuntimeError>(
        LoadedPackage(std::move(sharedPackage), std::move(instance).value()));
}

base::Expected<FrameSubmission, RuntimeError> Submit(
    LoadedPackage& loaded,
    IPackageExecutor& executor,
    const FrameInvocation& invocation)
{
    return executor.Submit(loaded.Instance(), invocation);
}

base::Expected<DeviceRecoveryReport, RuntimeError> RecoverDevice(
    LoadedPackage& loaded,
    IPackageExecutor& executor,
    DeviceRecoveryMode mode)
{
    return executor.RecoverDevice(loaded.Instance(), mode);
}
}
