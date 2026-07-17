#include "../46_CompositionContractTests/TestFixture.h"
#include "../19_PackageLinker/FrozenComposition.h"
#include "../19_PackageLinker/PackageLinker.h"
#include "../29_CompositionRuntime/CompositionRuntime.h"
#include "../15_PlatformWin32/Win32Window.h"

#include <Windows.h>

#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>

namespace
{
std::vector<std::byte> Bytes(const std::array<float, 4>& value)
{
    std::vector<std::byte> bytes(sizeof(value));
    std::memcpy(bytes.data(), value.data(), sizeof(value));
    return bytes;
}

bool Equals(std::span<const std::byte> bytes, const std::array<float, 4>& expected)
{
    if (bytes.size() < sizeof(expected)) return false;
    std::array<float, 4> actual{};
    std::memcpy(actual.data(), bytes.data(), sizeof(actual));
    for (std::size_t index = 0; index < actual.size(); ++index)
        if (std::abs(actual[index] - expected[index]) > 0.0001f) return false;
    return true;
}

int RunFrozen(const std::filesystem::path& path)
{
    namespace c = sge4::composition;
    namespace cr = sge4::composition::runtime_v1;
    std::ifstream inputFile(path, std::ios::binary | std::ios::ate);
    if (!inputFile) return 101;
    const auto size = inputFile.tellg();
    if (size <= 0) return 102;
    std::vector<std::byte> bytes(static_cast<std::size_t>(size));
    inputFile.seekg(0);
    inputFile.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    auto artifact = c::ReadFrozenComposition(std::move(bytes));
    if (!artifact) return 103;
    const std::array<float, 4> initial{1.0f, 2.0f, 3.0f, 4.0f};
    const std::array<float, 4> expected{7.0f, 9.0f, 11.0f, 13.0f};
    sge4::d3d12::D3D12Backend backend({true, false});
    cr::CompositionLoadInput loadInput;
    loadInput.initialResources.push_back({c::SharedResourceId{0}, Bytes(initial)});
    auto loaded = cr::LoadComposition(std::move(artifact).Value(), backend, std::move(loadInput));
    if (!loaded) return 104;
    auto submitted = cr::SubmitComposition(loaded.Value(), {0, {}});
    auto readback = cr::ReadCompositionBuffer(loaded.Value(), c::SharedResourceId{4});
    return submitted && readback && Equals(readback.Value().bytes, expected) ? 0 : 105;
}

int RunFreshProcess(const std::filesystem::path& artifactPath)
{
    std::array<wchar_t, 32768> executable{};
    const DWORD length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
    if (length == 0 || length >= executable.size()) return 106;
    std::wstring command = L"\"";
    command.append(executable.data(), length);
    command += L"\" --fresh \"";
    command += artifactPath.wstring();
    command += L"\"";
    std::vector<wchar_t> mutableCommand(command.begin(), command.end());
    mutableCommand.push_back(L'\0');
    STARTUPINFOW startup{}; startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(nullptr, mutableCommand.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup, &process)) return 107;
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exitCode = 0;
    const BOOL queried = GetExitCodeProcess(process.hProcess, &exitCode);
    CloseHandle(process.hThread); CloseHandle(process.hProcess);
    return queried ? static_cast<int>(exitCode) : 108;
}

int RunPresentingComposition()
{
    namespace c = sge4::composition;
    namespace cr = sge4::composition::runtime_v1;
    auto contract=sge4::qualification::composition_fixture::BuildPresentContract(); if(!contract)return 109;
    auto plan=c::ProposeLinkPlan(contract.Value()); if(!plan)return 110;
    auto verified=c::VerifyAndSeal(contract.Value(),std::move(plan).Value());
    if(!verified){std::cerr<<verified.Error().stage<<": "<<verified.Error().message<<'\n';return 111;}
    const auto& schedule=verified.Value().Plan().schedule;
    if(schedule.size()!=2||contract.Value().leaves[schedule.back().leaf.value].surfaceSlotCount!=1)return 112;
    auto bytes=c::WriteFrozenComposition(contract.Value(),verified.Value()); if(!bytes)return 113;
    auto artifact=c::ReadFrozenComposition(std::move(bytes).Value()); if(!artifact)return 114;
    auto window=sge4::platform::Win32Window::Create(L"SGE4 Level 4 v1 Presenting Composition",64,64); if(!window)return 115;
    sge4::d3d12::D3D12Backend backend({true,false}); cr::CompositionLoadInput loadInput;
    loadInput.surface=window.Value().get(); loadInput.initialResources.push_back({c::SharedResourceId{0},Bytes({0.2f,0.4f,0.8f,1.0f})});
    auto loaded=cr::LoadComposition(std::move(artifact).Value(),backend,std::move(loadInput)); if(!loaded)return 116;
    auto submitted=cr::SubmitComposition(loaded.Value(),{0,{}}); return submitted&&submitted.Value().leaves.size()==2?0:117;
}

int RunIndependentComposition()
{
    namespace c=sge4::composition; namespace cr=sge4::composition::runtime_v1;
    auto contract=sge4::qualification::composition_fixture::BuildIndependentContract(); if(!contract)return 118;
    auto plan=c::ProposeLinkPlan(contract.Value()); if(!plan)return 119;
    auto verified=c::VerifyAndSeal(contract.Value(),std::move(plan).Value()); if(!verified)return 120;
    auto bytes=c::WriteFrozenComposition(contract.Value(),verified.Value()); if(!bytes)return 121;
    auto artifact=c::ReadFrozenComposition(std::move(bytes).Value()); if(!artifact)return 122;
    sge4::d3d12::D3D12Backend backend({true,false}); cr::CompositionLoadInput input;
    input.initialResources={{c::SharedResourceId{0},Bytes({1,2,3,4})},{c::SharedResourceId{2},Bytes({4,3,2,1})}};
    auto loaded=cr::LoadComposition(std::move(artifact).Value(),backend,std::move(input)); if(!loaded)return 123;
    auto submitted=cr::SubmitComposition(loaded.Value(),{0,{}}); if(!submitted||submitted.Value().leaves.size()!=2)return 124;
    auto first=cr::ReadCompositionBuffer(loaded.Value(),c::SharedResourceId{1});
    auto second=cr::ReadCompositionBuffer(loaded.Value(),c::SharedResourceId{3});
    return first&&second&&Equals(first.Value().bytes,{2,3,4,5})&&Equals(second.Value().bytes,{5,4,3,2})?0:125;
}
}

int wmain(int argc, wchar_t** argv)
{
    if (argc == 3 && std::wstring_view(argv[1]) == L"--fresh") return RunFrozen(argv[2]);
    namespace c = sge4::composition;
    namespace cr = sge4::composition::runtime_v1;
    auto contract = sge4::qualification::composition_fixture::BuildDiamondContract();
    if (!contract) { std::cerr << contract.Error() << '\n'; return 1; }
    auto plan = c::ProposeLinkPlan(contract.Value());
    if (!plan) { std::cerr << plan.Error().message << '\n'; return 2; }
    auto verified = c::VerifyAndSeal(contract.Value(), std::move(plan).Value());
    if (!verified) { std::cerr << verified.Error().stage << ": " << verified.Error().message << '\n'; return 3; }
    auto bytes = c::WriteFrozenComposition(contract.Value(), verified.Value());
    if (!bytes) { std::cerr << bytes.Error().message << '\n'; return 4; }
    const auto artifactPath = std::filesystem::absolute("build/tests/SGE4_Level4v1_FreshProcess.sgec");
    std::filesystem::create_directories(artifactPath.parent_path());
    {
        std::ofstream output(artifactPath, std::ios::binary | std::ios::trunc);
        output.write(reinterpret_cast<const char*>(bytes.Value().data()), static_cast<std::streamsize>(bytes.Value().size()));
        if (!output) return 5;
    }
    auto artifact = c::ReadFrozenComposition(std::move(bytes).Value());
    if (!artifact) { std::cerr << artifact.Error().message << '\n'; return 6; }

    const std::array<float, 4> input{1.0f, 2.0f, 3.0f, 4.0f};
    const std::array<float, 4> expected{7.0f, 9.0f, 11.0f, 13.0f};
    sge4::d3d12::D3D12Backend backend({true, false});
    cr::CompositionLoadInput loadInput;
    loadInput.initialResources.push_back({c::SharedResourceId{0}, Bytes(input)});
    auto loaded = cr::LoadComposition(std::move(artifact).Value(), backend, std::move(loadInput));
    if (!loaded) { std::cerr << loaded.Error().stage << ": " << loaded.Error().message << '\n'; return 7; }
    if (loaded.Value().LeafCount() != 4) return 8;

    auto submission = cr::SubmitComposition(loaded.Value(), {0, {}});
    if (!submission) { std::cerr << submission.Error().stage << ": " << submission.Error().message << '\n'; return 9; }
    if (submission.Value().leaves.size() != 4) return 10;
    for (const auto& leaf : submission.Value().leaves)
        if (leaf.deviceEpoch != submission.Value().deviceEpoch) return 11;
    auto readback = cr::ReadCompositionBuffer(loaded.Value(), c::SharedResourceId{4});
    if (!readback || !Equals(readback.Value().bytes, expected)) return 12;
    const int independentResult=RunIndependentComposition();
    if(independentResult!=0){std::cerr<<"independent composition failed: "<<independentResult<<'\n';return 22;}
    const int presentingResult=RunPresentingComposition();
    if(presentingResult!=0){std::cerr<<"single-presenter composition failed: "<<presentingResult<<'\n';return 21;}

    const auto oldEpoch = loaded.Value().DeviceEpoch();
    const auto oldResource = loaded.Value().SharedResource(c::SharedResourceId{4});
    const auto oldToken = loaded.Value().AvailableAfter(c::SharedResourceId{4});
    auto recovery = cr::RecoverComposition(loaded.Value(), sge4::runtime::DeviceRecoveryMode::ControlledRebuild);
    if (!recovery) { std::cerr << recovery.Error().stage << ": " << recovery.Error().message << '\n'; return 13; }
    if (loaded.Value().DeviceEpoch() <= oldEpoch ||
        recovery.Value().previousDeviceEpoch != oldEpoch ||
        recovery.Value().newDeviceEpoch != loaded.Value().DeviceEpoch() ||
        !recovery.Value().packageObjectsRebuilt || !recovery.Value().temporalHistoryReset) return 14;
    if (!oldResource || !oldToken || oldResource->DeviceEpoch() == loaded.Value().DeviceEpoch() ||
        oldToken->DeviceEpoch() == loaded.Value().DeviceEpoch()) return 15;

    auto recoveredSubmission = cr::SubmitComposition(loaded.Value(), {1, {}});
    auto recoveredReadback = cr::ReadCompositionBuffer(loaded.Value(), c::SharedResourceId{4});
    if (!recoveredSubmission || !recoveredReadback || !Equals(recoveredReadback.Value().bytes, expected)) return 16;

    auto forced = cr::RecoverComposition(loaded.Value(), sge4::runtime::DeviceRecoveryMode::ForceRemovalForTest);
    if (!forced) { std::cerr << forced.Error().stage << ": " << forced.Error().message << '\n'; return 17; }
    if (!forced.Value().forcedRemoval || forced.Value().stateAfter != sge4::runtime::DeviceRuntimeState::AwaitingAdapter ||
        forced.Value().newDeviceEpoch != loaded.Value().DeviceEpoch() || forced.Value().packageObjectsRebuilt) return 18;
    auto inactiveSubmission = cr::SubmitComposition(loaded.Value(), {2, {}});
    if (inactiveSubmission || inactiveSubmission.Error().stage != "composition/device-state") return 19;
    const int childResult = RunFreshProcess(artifactPath);
    std::filesystem::remove(artifactPath);
    if (childResult != 0) { std::cerr << "fresh-process composition rematerialization failed: " << childResult << '\n'; return 20; }

    std::cout << "Level 4 v1 WARP independent, diamond, and single-presenter compositions, domain recovery, actual removal, and fresh-process rematerialization tests passed\n";
    return 0;
}
