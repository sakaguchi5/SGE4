#include "../11_D3D12PackageLowering/D3D12PackageLowering.h"
#include "../08_CandidatePlanner/CandidatePlanner.h"
#include "../09_FrozenPackageCore/PackageReader.h"
#include "../13_PackageRuntime/PackageRuntime.h"
#include "../14_D3D12Backend/D3D12Backend.h"
#include "../07_ExecutionPlanVerifier/ExecutionPlanVerifier.h"
#include "../27_RuntimeScenarios/RuntimeScenarios.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <iostream>
#include <span>
#include <vector>

namespace
{
namespace k = sge4_5::qualification::runtime_scenarios;
namespace compiler = sge4_5::compiler::d3d12;
namespace l3c = sge4_5::compiler::d3d12::candidate;
namespace l3 = sge4_5::planning;

std::span<const std::byte> Bytes(const k::Float4& value) { return std::as_bytes(std::span<const float>(value)); }

k::Float4 Decode(std::span<const std::byte> bytes)
{
    k::Float4 result{};
    if (bytes.size() >= sizeof(result)) std::memcpy(result.data(), bytes.data(), sizeof(result));
    return result;
}

bool Near(const k::Float4& left, const k::Float4& right)
{
    for (std::size_t index = 0; index < left.size(); ++index)
        if (std::abs(left[index] - right[index]) > 1.0e-5f) return false;
    return true;
}

std::shared_ptr<sge4_5::runtime::ICompletionToken> ReleaseToken(
    const sge4_5::runtime::FrameSubmission& submission, std::uint32_t slot)
{
    const auto found = std::find_if(submission.releasedExternalResources.begin(),
        submission.releasedExternalResources.end(), [slot](const auto& release) { return release.slot == slot; });
    return found == submission.releasedExternalResources.end() ? nullptr : found->safeAfter;
}

sge4_5::base::Result<k::Float4, std::string> Execute(
    const k::Input& input, const l3::verification::VerifiedExecutionPlan& plan, bool recover)
{
    auto compiled = compiler::LowerVerifiedPlan(input.graph, input.targetProfile, plan);
    if (!compiled) return sge4_5::base::Result<k::Float4, std::string>::Failure(
        compiled.Error().stage + ": " + compiled.Error().message);
    auto package = sge4_5::package::PackageReader::Read(std::move(compiled).Value().packageBytes);
    if (!package) return sge4_5::base::Result<k::Float4, std::string>::Failure(package.Error().message);
    sge4_5::d3d12::D3D12Backend executor({true, true});
    auto loaded = sge4_5::runtime::LoadPackage(std::move(package).Value(), executor);
    if (!loaded) return sge4_5::base::Result<k::Float4, std::string>::Failure(
        loaded.Error().stage + ": " + loaded.Error().message);

    if (recover)
    {
        auto recovered = sge4_5::runtime::RecoverDevice(loaded.Value(), executor,
            sge4_5::runtime::DeviceRecoveryMode::ControlledRebuild);
        if (!recovered || !recovered.Value().packageObjectsRebuilt || !recovered.Value().externalRebindRequired)
            return sge4_5::base::Result<k::Float4, std::string>::Failure("selected Package reconstruction failed");
    }

    const k::Float4 externalInput{0.5f, 1.0f, 1.5f, 2.0f};
    const k::Float4 dynamicA{2.0f, 4.0f, 6.0f, 8.0f};
    const k::Float4 dynamicB{1.0f, 1.5f, 2.0f, 2.5f};
    const k::Float4 zero{};
    auto inputBuffer = executor.CreateExternalBuffer(loaded.Value().Instance(), 0, Bytes(externalInput));
    auto outputA = executor.CreateExternalBuffer(loaded.Value().Instance(), 1, Bytes(zero));
    auto outputB = executor.CreateExternalBuffer(loaded.Value().Instance(), 2, Bytes(zero));
    if (!inputBuffer || !outputA || !outputB)
        return sge4_5::base::Result<k::Float4, std::string>::Failure("external Buffer creation failed");
    const sge4_5::runtime::DynamicDataBinding dynamic[2] = {{0, Bytes(dynamicA)}, {1, Bytes(dynamicB)}};
    const sge4_5::runtime::ExternalResourceBinding external[3] = {
        {0, inputBuffer.Value().resource, inputBuffer.Value().availableAfter},
        {1, outputA.Value().resource, outputA.Value().availableAfter},
        {2, outputB.Value().resource, outputB.Value().availableAfter}};
    sge4_5::runtime::FrameInvocation invocation{0, dynamic, external};
    auto submitted = sge4_5::runtime::Submit(loaded.Value(), executor, invocation);
    if (!submitted) return sge4_5::base::Result<k::Float4, std::string>::Failure(
        submitted.Error().stage + ": " + submitted.Error().message);
    auto readback = executor.ReadExternalBuffer(loaded.Value().Instance(), outputB.Value().resource,
        ReleaseToken(submitted.Value(), 2));
    if (!readback) return sge4_5::base::Result<k::Float4, std::string>::Failure(
        readback.Error().stage + ": " + readback.Error().message);
    const auto actual = Decode(readback.Value().bytes);
    const auto expected = k::PipelineExpected(dynamicA, dynamicB, externalInput);
    if (!Near(actual, expected)) return sge4_5::base::Result<k::Float4, std::string>::Failure(
        "GPU observation differs from the Semantic reference");
    return sge4_5::base::Result<k::Float4, std::string>::Success(actual);
}
}

int main()
{
    auto built = k::Build(k::Scenario::DynamicExternalPipeline);
    if (!built) { std::cerr << built.Error() << '\n'; return 1; }
    auto input = std::move(built).Value();
    auto validated = compiler::ValidateSourceStage(input.graph, input.targetProfile);
    if (!validated) return 2;
    auto obligation = l3::BuildSemanticObligation(input.graph, validated.Value().analyzed);
    if (!obligation) return 2;
    const auto contract = l3::BuildPlanningContract(input.targetProfile);
    l3::CompilerPolicy policy;
    auto plans = l3c::GenerateCandidatePlans(obligation.Value(), contract, policy);

    std::vector<k::Float4> observations;
    bool scheduleAlternative = false;
    bool allDirect = false;
    bool dedicated = false;
    bool recovered = false;
    for (const auto& plan : plans)
    {
        auto sealed = l3::verification::VerifyAndSeal(obligation.Value(), contract, plan);
        if (!sealed) continue;
        scheduleAlternative |= plan.scheduleStrategy != l3::ScheduleStrategy::CanonicalMinimumId;
        allDirect |= plan.queueStrategy == l3::QueueStrategy::AllDirect;
        dedicated |= plan.queueStrategy == l3::QueueStrategy::CanonicalSafe ||
                     plan.queueStrategy == l3::QueueStrategy::KindPreferredDedicated;
        auto observed = Execute(input, sealed.Value(), !recovered);
        if (!observed)
        {
            std::cerr << "verified Plan execution failed: " << observed.Error() << '\n';
            return 3;
        }
        recovered = true;
        observations.push_back(observed.Value());
    }
    if (observations.size() < 2 || !scheduleAlternative || !allDirect || !dedicated || !recovered)
    {
        std::cerr << "runtime candidate corpus did not cover schedule/Queue/reconstruction axes\n";
        return 4;
    }
    if (!std::all_of(observations.begin(), observations.end(), [&](const auto& value) {
        return Near(value, observations.front());
    }))
    {
        std::cerr << "verified Plans produced different GPU observations\n";
        return 5;
    }
    std::cout << "SGE4 verified schedule and Queue Packages produced identical WARP observations.\n";
    std::cout << "Selected Package reconstruction preserved the observation contract.\n";
    return 0;
}
