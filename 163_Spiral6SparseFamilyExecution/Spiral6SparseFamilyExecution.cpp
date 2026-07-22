#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "Spiral6SparseFamilyExecution.h"

#include "../00_Foundation/BinaryIO.h"

#if defined(_WIN32)
#include <Windows.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#endif

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <map>
#include <set>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <utility>

#if defined(_WIN32)
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib")
#endif

namespace sge4_5::spiral6::family_execution
{
namespace fc = family_candidate;
namespace fv = family_verification;
namespace
{
SparseFamilyExecutionErrorV1 Error(std::string stage, std::string message, long nativeCode = 0)
{
    return {std::move(stage), std::move(message), nativeCode};
}
base::Digest256 Literal(std::string_view value)
{
    return base::Sha256(std::as_bytes(std::span(value.data(), value.size())));
}
void Digest(base::BinaryWriter& writer, const base::Digest256& value) { writer.WriteBytes(value); }

base::Digest256 BindingIdentity(
    const fv::VerifiedSparseFamilyCandidateV1& verified,
    const SparseFamilyResourceBindingInputV1& binding)
{
    base::BinaryWriter writer;
    Digest(writer, Literal("SGE4-5.Spiral6.FrozenVerifiedSparseFamilyBinding.V1"));
    Digest(writer, verified.Plan().planIdentity);
    Digest(writer, verified.CertificateIdentity());
    writer.WriteU64(binding.deviceEpoch);
    Digest(writer, binding.actualRepresentationResourceIdentity);
    writer.WriteU32(static_cast<std::uint32_t>(binding.representationRole));
    writer.WriteU32(binding.fixedCapacityBytes);
    writer.WriteU32(binding.payloadStrideBytes);
    return base::Sha256(writer.Bytes());
}

#if defined(_WIN32)
using Microsoft::WRL::ComPtr;

std::string HResultText(HRESULT hr)
{
    std::ostringstream stream;
    stream << "HRESULT 0x" << std::hex << std::uppercase << static_cast<unsigned long>(hr);
    return stream.str();
}
void Check(HRESULT hr, std::string_view stage)
{
    if (FAILED(hr)) throw std::runtime_error(std::string(stage) + ": " + HResultText(hr));
}
std::string WideToUtf8(const wchar_t* text)
{
    if (!text) return {};
    const int count = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    if (count <= 1) return {};
    std::string value(static_cast<std::size_t>(count), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text, -1, value.data(), count, nullptr, nullptr);
    value.pop_back();
    return value;
}
struct EventHandle final
{
    HANDLE value = nullptr;
    EventHandle() : value(CreateEventW(nullptr, FALSE, FALSE, nullptr))
    {
        if (!value) throw std::runtime_error("CreateEventW failed.");
    }
    ~EventHandle() { if (value) CloseHandle(value); }
    EventHandle(const EventHandle&) = delete;
    EventHandle& operator=(const EventHandle&) = delete;
};

ComPtr<ID3DBlob> CompileShader(std::string_view source, std::string_view name)
{
    ComPtr<ID3DBlob> shader;
    ComPtr<ID3DBlob> errors;
    const UINT flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS | D3DCOMPILE_OPTIMIZATION_LEVEL3;
    const HRESULT hr = D3DCompile(source.data(), source.size(), std::string(name).c_str(), nullptr, nullptr,
        "main", "cs_5_1", flags, 0, &shader, &errors);
    if (FAILED(hr))
    {
        const std::string message = errors
            ? std::string(static_cast<const char*>(errors->GetBufferPointer()), errors->GetBufferSize())
            : HResultText(hr);
        throw std::runtime_error("D3DCompile: " + message);
    }
    return shader;
}

ComPtr<ID3D12Resource> CreateBuffer(ID3D12Device* device, D3D12_HEAP_TYPE heapType,
    std::uint64_t size, D3D12_RESOURCE_STATES state, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE)
{
    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = heapType;
    heap.CreationNodeMask = 1;
    heap.VisibleNodeMask = 1;
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = size;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    desc.Flags = flags;
    ComPtr<ID3D12Resource> resource;
    Check(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, state, nullptr, IID_PPV_ARGS(&resource)),
        "CreateCommittedResource");
    return resource;
}
void UploadBytes(ID3D12Resource* resource, std::span<const std::byte> bytes)
{
    void* mapped = nullptr;
    D3D12_RANGE noRead{0, 0};
    Check(resource->Map(0, &noRead, &mapped), "Map(upload)");
    std::memcpy(mapped, bytes.data(), bytes.size());
    resource->Unmap(0, nullptr);
}
std::vector<std::byte> ReadbackBytes(ID3D12Resource* resource, std::size_t size)
{
    void* mapped = nullptr;
    D3D12_RANGE range{0, size};
    Check(resource->Map(0, &range, &mapped), "Map(readback)");
    const auto* first = static_cast<const std::byte*>(mapped);
    std::vector<std::byte> bytes(first, first + size);
    D3D12_RANGE noWrite{0, 0};
    resource->Unmap(0, &noWrite);
    return bytes;
}
D3D12_RESOURCE_BARRIER Transition(ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
{
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = resource;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter = after;
    return barrier;
}
D3D12_RESOURCE_BARRIER UavBarrier(ID3D12Resource* resource)
{
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barrier.UAV.pResource = resource;
    return barrier;
}

std::string CommonTransformBody()
{
    return R"(
struct Motor { float4 qr; float4 qd; };
cbuffer SparseParameters : register(b0) { uint activeCount; };
StructuredBuffer<uint> Representation : register(t0);
StructuredBuffer<Motor> GlobalMotors : register(t1);
StructuredBuffer<float4> Points : register(t2);
RWStructuredBuffer<float4> Output : register(u0);
float4 qmul(float4 a, float4 b)
{
    return float4(
        a.w*b.x + b.w*a.x + a.y*b.z - a.z*b.y,
        a.w*b.y + b.w*a.y + a.z*b.x - a.x*b.z,
        a.w*b.z + b.w*a.z + a.x*b.y - a.y*b.x,
        a.w*b.w - dot(a.xyz,b.xyz));
}
float4 qconj(float4 q) { return float4(-q.xyz, q.w); }
void writeTransform(uint universeIndex)
{
    uint bone = universeIndex / 512;
    Motor motor = GlobalMotors[bone];
    float4 inputPoint = Points[universeIndex];
    float4 conjugate = qconj(motor.qr);
    float4 rotated = qmul(qmul(motor.qr, float4(inputPoint.xyz,0)), conjugate);
    float3 translation = 2.0f * qmul(motor.qd, conjugate).xyz;
    Output[universeIndex] = float4(rotated.xyz + translation, 1.0f);
}
)";
}
std::string DenseShader()
{
    return CommonTransformBody() + R"(
[numthreads(64,1,1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    if (activeCount > 4096) return;
    uint universeIndex = dispatchThreadId.x;
    if (universeIndex >= 4096) return;
    uint word = Representation[universeIndex >> 5];
    if ((word & (1u << (universeIndex & 31u))) == 0) return;
    writeTransform(universeIndex);
}
)";
}
std::string CompactShader()
{
    return CommonTransformBody() + R"(
[numthreads(64,1,1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint ordinal = dispatchThreadId.x;
    if (ordinal >= activeCount) return;
    uint universeIndex = Representation[ordinal];
    if (universeIndex >= 4096) return;
    writeTransform(universeIndex);
}
)";
}
std::string ActiveBlockShader()
{
    return CommonTransformBody() + R"(
[numthreads(64,1,1)]
void main(uint3 groupId : SV_GroupID, uint3 groupThreadId : SV_GroupThreadID)
{
    if (activeCount > 4096) return;
    uint base = groupId.x * 4;
    uint blockIndex = Representation[base + 0];
    uint mask = groupThreadId.x < 32 ? Representation[base + 1] : Representation[base + 2];
    uint bit = groupThreadId.x & 31u;
    if ((mask & (1u << bit)) == 0) return;
    uint universeIndex = blockIndex * 64 + groupThreadId.x;
    if (universeIndex >= 4096) return;
    writeTransform(universeIndex);
}
)";
}

struct GpuContext final
{
    ComPtr<IDXGIFactory6> factory;
    ComPtr<IDXGIAdapter> adapter;
    ComPtr<ID3D12Device> device;
    ComPtr<ID3D12CommandQueue> queue;
    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12GraphicsCommandList> commandList;
    ComPtr<ID3D12Fence> fence;
    EventHandle fenceEvent;
    std::uint64_t fenceValue = 0;
    std::string adapterDescription;
    LUID adapterLuid{};
    ComPtr<ID3D12RootSignature> rootSignature;
    std::map<fc::SparseFamilyCandidateKindV1, ComPtr<ID3D12PipelineState>> pipelines;
    ComPtr<ID3D12CommandSignature> dispatchSignature;

    GpuContext()
    {
        Check(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)), "CreateDXGIFactory2");
        ComPtr<IDXGIAdapter1> warp;
        Check(factory->EnumWarpAdapter(IID_PPV_ARGS(&warp)), "EnumWarpAdapter");
        adapter = warp;
        DXGI_ADAPTER_DESC1 description{};
        Check(warp->GetDesc1(&description), "GetDesc1(WARP)");
        adapterDescription = WideToUtf8(description.Description);
        adapterLuid = description.AdapterLuid;
        Check(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)), "D3D12CreateDevice(WARP)");
        D3D12_COMMAND_QUEUE_DESC queueDesc{};
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        Check(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&queue)), "CreateCommandQueue");
        Check(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator)), "CreateCommandAllocator");
        Check(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr, IID_PPV_ARGS(&commandList)), "CreateCommandList");
        Check(commandList->Close(), "Initial Close");
        Check(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)), "CreateFence");

        std::array<D3D12_ROOT_PARAMETER, 5> parameters{};
        parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        parameters[0].Constants.ShaderRegister = 0;
        parameters[0].Constants.Num32BitValues = 1;
        parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        for (std::uint32_t i = 0; i < 3; ++i)
        {
            parameters[i + 1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
            parameters[i + 1].Descriptor.ShaderRegister = i;
            parameters[i + 1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        }
        parameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
        parameters[4].Descriptor.ShaderRegister = 0;
        parameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        D3D12_ROOT_SIGNATURE_DESC rootDesc{};
        rootDesc.NumParameters = static_cast<UINT>(parameters.size());
        rootDesc.pParameters = parameters.data();
        ComPtr<ID3DBlob> serialized;
        ComPtr<ID3DBlob> errors;
        const HRESULT rootResult = D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &errors);
        if (FAILED(rootResult))
        {
            const std::string message = errors
                ? std::string(static_cast<const char*>(errors->GetBufferPointer()), errors->GetBufferSize())
                : HResultText(rootResult);
            throw std::runtime_error("D3D12SerializeRootSignature: " + message);
        }
        Check(device->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(), IID_PPV_ARGS(&rootSignature)), "CreateRootSignature");
        for (const auto [kind, source, name] : {
            std::tuple{fc::SparseFamilyCandidateKindV1::DenseMembershipMaskPredicate, DenseShader(), std::string{"DenseMask"}},
            std::tuple{fc::SparseFamilyCandidateKindV1::CompactIndexListDispatch, CompactShader(), std::string{"CompactIndex"}},
            std::tuple{fc::SparseFamilyCandidateKindV1::ActiveBlockListLocalMask, ActiveBlockShader(), std::string{"ActiveBlock"}}})
        {
            const auto shader = CompileShader(source, name);
            D3D12_COMPUTE_PIPELINE_STATE_DESC pipelineDesc{};
            pipelineDesc.pRootSignature = rootSignature.Get();
            pipelineDesc.CS = {shader->GetBufferPointer(), shader->GetBufferSize()};
            ComPtr<ID3D12PipelineState> pipeline;
            Check(device->CreateComputePipelineState(&pipelineDesc, IID_PPV_ARGS(&pipeline)), "CreateComputePipelineState");
            pipelines.emplace(kind, std::move(pipeline));
        }
        D3D12_INDIRECT_ARGUMENT_DESC argumentDesc{};
        argumentDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;
        D3D12_COMMAND_SIGNATURE_DESC signatureDesc{};
        signatureDesc.ByteStride = sizeof(D3D12_DISPATCH_ARGUMENTS);
        signatureDesc.NumArgumentDescs = 1;
        signatureDesc.pArgumentDescs = &argumentDesc;
        Check(device->CreateCommandSignature(&signatureDesc, nullptr, IID_PPV_ARGS(&dispatchSignature)), "CreateCommandSignature");
    }

    void Begin()
    {
        Check(allocator->Reset(), "CommandAllocator::Reset");
        Check(commandList->Reset(allocator.Get(), nullptr), "CommandList::Reset");
    }
    void SubmitAndWait()
    {
        Check(commandList->Close(), "CommandList::Close");
        ID3D12CommandList* lists[]{commandList.Get()};
        queue->ExecuteCommandLists(1, lists);
        const auto next = ++fenceValue;
        Check(queue->Signal(fence.Get(), next), "Queue::Signal");
        if (fence->GetCompletedValue() < next)
        {
            Check(fence->SetEventOnCompletion(next, fenceEvent.value), "Fence::SetEventOnCompletion");
            WaitForSingleObject(fenceEvent.value, INFINITE);
        }
    }
};

struct ExecutedCandidate final
{
    SparseFamilyCandidateObservationV1 observation;
    std::vector<std::byte> outputBytes;
};

ExecutedCandidate ExecuteCandidate(
    GpuContext& gpu,
    const semantic::SparsePointTransformSemanticV1& semanticValue,
    const semantic::ExactSparseWorkSetV1& set,
    const FrozenVerifiedSparseFamilyCandidateV1& frozen)
{
    auto artifactValid = fc::ValidateSparseFamilyArtifactV1(frozen.Artifact(), semanticValue, set);
    if (!artifactValid) throw std::runtime_error(artifactValid.Error());
    const auto motors = semantic::BuildCanonicalGlobalMotorPaletteV1();
    const auto motorBytes = spiral5::semantic::SerializeMotorRecordsV1(motors);
    const auto points = spiral5::semantic::BuildCanonicalPointCorpusV1();
    const auto pointBytes = spiral5::semantic::SerializePointRecordsV1(points);
    const auto zeroBytes = semantic::BuildCanonicalInactiveOutputBytesV1();
    const auto expected = semantic::BuildSparseCpuReferenceOutputV1(set);

    auto representationUpload = CreateBuffer(gpu.device.Get(), D3D12_HEAP_TYPE_UPLOAD,
        frozen.Artifact().PayloadBytes().size(), D3D12_RESOURCE_STATE_GENERIC_READ);
    auto motorUpload = CreateBuffer(gpu.device.Get(), D3D12_HEAP_TYPE_UPLOAD, motorBytes.size(), D3D12_RESOURCE_STATE_GENERIC_READ);
    auto pointUpload = CreateBuffer(gpu.device.Get(), D3D12_HEAP_TYPE_UPLOAD, pointBytes.size(), D3D12_RESOURCE_STATE_GENERIC_READ);
    auto zeroUpload = CreateBuffer(gpu.device.Get(), D3D12_HEAP_TYPE_UPLOAD, zeroBytes.size(), D3D12_RESOURCE_STATE_GENERIC_READ);
    const std::array<std::uint32_t, 3> dispatchArguments{frozen.Artifact().DispatchGroupsX(), 1u, 1u};
    const auto argumentBytes = std::as_bytes(std::span(dispatchArguments));
    auto argumentUpload = CreateBuffer(gpu.device.Get(), D3D12_HEAP_TYPE_UPLOAD, argumentBytes.size(), D3D12_RESOURCE_STATE_GENERIC_READ);
    auto output = CreateBuffer(gpu.device.Get(), D3D12_HEAP_TYPE_DEFAULT, zeroBytes.size(), D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    auto readback = CreateBuffer(gpu.device.Get(), D3D12_HEAP_TYPE_READBACK, zeroBytes.size(), D3D12_RESOURCE_STATE_COPY_DEST);
    UploadBytes(representationUpload.Get(), frozen.Artifact().PayloadBytes());
    UploadBytes(motorUpload.Get(), motorBytes);
    UploadBytes(pointUpload.Get(), pointBytes);
    UploadBytes(zeroUpload.Get(), zeroBytes);
    UploadBytes(argumentUpload.Get(), argumentBytes);

    gpu.Begin();
    gpu.commandList->CopyBufferRegion(output.Get(), 0, zeroUpload.Get(), 0, zeroBytes.size());
    auto toUav = Transition(output.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    gpu.commandList->ResourceBarrier(1, &toUav);
    gpu.commandList->SetPipelineState(gpu.pipelines.at(frozen.Artifact().Kind()).Get());
    gpu.commandList->SetComputeRootSignature(gpu.rootSignature.Get());
    gpu.commandList->SetComputeRoot32BitConstant(0, frozen.Artifact().ActiveCount(), 0);
    gpu.commandList->SetComputeRootShaderResourceView(1, representationUpload->GetGPUVirtualAddress());
    gpu.commandList->SetComputeRootShaderResourceView(2, motorUpload->GetGPUVirtualAddress());
    gpu.commandList->SetComputeRootShaderResourceView(3, pointUpload->GetGPUVirtualAddress());
    gpu.commandList->SetComputeRootUnorderedAccessView(4, output->GetGPUVirtualAddress());
    gpu.commandList->ExecuteIndirect(gpu.dispatchSignature.Get(), 1, argumentUpload.Get(), 0, nullptr, 0);
    auto uav = UavBarrier(output.Get());
    gpu.commandList->ResourceBarrier(1, &uav);
    auto toCopy = Transition(output.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
    gpu.commandList->ResourceBarrier(1, &toCopy);
    gpu.commandList->CopyBufferRegion(readback.Get(), 0, output.Get(), 0, zeroBytes.size());
    gpu.SubmitAndWait();

    ExecutedCandidate result;
    result.outputBytes = ReadbackBytes(readback.Get(), zeroBytes.size());
    std::vector<spiral5::semantic::PointRecordV1> observed(semantic::WorkUniverseCountV1);
    std::memcpy(observed.data(), result.outputBytes.data(), result.outputBytes.size());
    auto& observation = result.observation;
    observation.kind = frozen.Artifact().Kind();
    observation.activeCount = frozen.Artifact().ActiveCount();
    observation.activeBlockCount = frozen.Artifact().ActiveBlockCount();
    observation.expectedDispatchGroupsX = frozen.Verified().Plan().operation.dispatchGroupsX;
    observation.observedDispatchGroupsX = frozen.Artifact().DispatchGroupsX();
    observation.artifactIdentity = frozen.Artifact().ArtifactIdentity();
    observation.bindingIdentity = frozen.BindingIdentity();
    observation.outputDigest = base::Sha256(result.outputBytes);
    observation.activeReferenceQualified = true;
    observation.inactiveSentinelByteStable = true;
    observation.activeHomogeneousWQualified = true;
    observation.finiteQualified = true;
    for (std::uint32_t index = 0; index < semantic::WorkUniverseCountV1; ++index)
    {
        if (!set.Contains(index))
        {
            const auto offset = static_cast<std::size_t>(index) * sizeof(spiral5::semantic::PointRecordV1);
            if (std::memcmp(result.outputBytes.data() + offset, zeroBytes.data() + offset,
                    sizeof(spiral5::semantic::PointRecordV1)) != 0)
                observation.inactiveSentinelByteStable = false;
            continue;
        }
        ++observation.activeWriteCount;
        const auto& actual = observed[index];
        const auto& reference = expected[index];
        const std::array<float, 4> a{actual.x, actual.y, actual.z, actual.w};
        const std::array<float, 4> e{reference.x, reference.y, reference.z, reference.w};
        for (std::size_t component = 0; component < a.size(); ++component)
        {
            if (!std::isfinite(a[component])) observation.finiteQualified = false;
            const double error = std::abs(static_cast<double>(a[component]) - e[component]);
            observation.maxAbsoluteError = std::max(observation.maxAbsoluteError, error);
            if (error > 1.0e-6) observation.activeReferenceQualified = false;
        }
        if (std::bit_cast<std::uint32_t>(actual.w) != std::bit_cast<std::uint32_t>(1.0f))
            observation.activeHomogeneousWQualified = false;
    }
    if (observation.observedDispatchGroupsX != observation.expectedDispatchGroupsX ||
        observation.activeWriteCount != observation.activeCount || !observation.activeReferenceQualified ||
        !observation.inactiveSentinelByteStable || !observation.activeHomogeneousWQualified || !observation.finiteQualified)
        throw std::runtime_error("Sparse Family WARP observation failed exact write-set qualification.");
    return result;
}

SparseCandidateFamilyCaseResultV1 RunVerifiedSparseCandidateFamilyWithGpuV1(
    GpuContext& gpu,
    const semantic::SparsePointTransformSemanticV1& semanticValue,
    semantic::SparsePatternKindV1 pattern,
    const semantic::ExactSparseWorkSetV1& set,
    const fv::SparseFamilyVerificationContextV1& context,
    const std::vector<FrozenVerifiedSparseFamilyCandidateV1>& frozen,
    std::uint64_t currentDeviceEpoch)
{
    if (frozen.size() != 3)
        throw std::runtime_error("Sparse Candidate Family requires exactly three Frozen Candidates.");
    std::set<fc::SparseFamilyCandidateKindV1> kinds;
    for (const auto& value : frozen)
    {
        auto valid = ValidateFrozenVerifiedSparseFamilyCandidateV1(
            value, semanticValue, set, context, currentDeviceEpoch);
        if (!valid) throw std::runtime_error(valid.Error());
        kinds.insert(value.Artifact().Kind());
    }
    if (kinds.size() != 3)
        throw std::runtime_error("Sparse Candidate Family kinds are not unique.");

    SparseCandidateFamilyCaseResultV1 result;
    result.adapterDescription = gpu.adapterDescription;
    result.pattern = pattern;
    result.semanticIdentity = semantic::SparsePointTransformSemanticIdentityV1(semanticValue);
    result.sparseSetIdentity = set.Identity();
    std::vector<std::vector<std::byte>> outputs;
    for (const auto& value : frozen)
    {
        auto executed = ExecuteCandidate(gpu, semanticValue, set, value);
        result.candidates.push_back(executed.observation);
        outputs.push_back(std::move(executed.outputBytes));
    }
    result.pairwiseOutputByteIdentical =
        outputs[0] == outputs[1] && outputs[1] == outputs[2];
    if (!result.pairwiseOutputByteIdentical)
        throw std::runtime_error("Sparse Candidate Family outputs are not byte-identical.");
    return result;
}
#endif
}

FrozenVerifiedSparseFamilyCandidateV1::FrozenVerifiedSparseFamilyCandidateV1(
    fv::VerifiedSparseFamilyCandidateV1 verified,
    fc::FrozenSparseFamilyArtifactV1 artifact,
    SparseFamilyResourceBindingInputV1 binding,
    base::Digest256 bindingIdentity)
    : verified_(std::move(verified)), artifact_(std::move(artifact)), binding_(binding), bindingIdentity_(bindingIdentity)
{
}

SparseFamilyResourceBindingInputV1 BuildCanonicalSparseFamilyResourceBindingInputV1(
    const fv::VerifiedSparseFamilyCandidateV1& verified,
    std::uint64_t deviceEpoch)
{
    SparseFamilyResourceBindingInputV1 binding;
    binding.deviceEpoch = deviceEpoch;
    binding.actualRepresentationResourceIdentity = verified.Plan().operation.representationResourceIdentity;
    binding.representationRole = verified.Plan().operation.representationRole;
    binding.fixedCapacityBytes = verified.Plan().operation.fixedCapacityBytes;
    binding.payloadStrideBytes = verified.Plan().operation.payloadStrideBytes;
    return binding;
}

base::Result<FrozenVerifiedSparseFamilyCandidateV1, std::string> FreezeVerifiedSparseFamilyCandidateV1(
    const semantic::SparsePointTransformSemanticV1& semanticValue,
    const semantic::ExactSparseWorkSetV1& set,
    const fv::SparseFamilyVerificationContextV1& context,
    const fv::VerifiedSparseFamilyCandidateV1& verified,
    const fc::FrozenSparseFamilyArtifactV1& artifact,
    const SparseFamilyResourceBindingInputV1& binding)
{
    auto verifiedValid = fv::ValidateVerifiedSparseFamilyCandidateV1(verified, semanticValue, set, context);
    if (!verifiedValid) return base::Result<FrozenVerifiedSparseFamilyCandidateV1, std::string>::Failure(verifiedValid.Error());
    auto artifactValid = fc::ValidateSparseFamilyArtifactV1(artifact, semanticValue, set);
    if (!artifactValid) return base::Result<FrozenVerifiedSparseFamilyCandidateV1, std::string>::Failure(artifactValid.Error());
    const auto& operation = verified.Plan().operation;
    if (artifact.ArtifactIdentity() != operation.artifactIdentity || artifact.Kind() != operation.kind ||
        artifact.Role() != operation.representationRole || artifact.DispatchGroupsX() != operation.dispatchGroupsX)
        return base::Result<FrozenVerifiedSparseFamilyCandidateV1, std::string>::Failure("Sparse Family artifact does not match Verified Plan.");
    if (binding.deviceEpoch == 0)
        return base::Result<FrozenVerifiedSparseFamilyCandidateV1, std::string>::Failure("Sparse Family binding requires non-zero device epoch.");
    if (binding.actualRepresentationResourceIdentity != operation.representationResourceIdentity)
        return base::Result<FrozenVerifiedSparseFamilyCandidateV1, std::string>::Failure("Sparse Family actual Resource identity mismatch.");
    if (binding.representationRole != operation.representationRole ||
        binding.fixedCapacityBytes != operation.fixedCapacityBytes ||
        binding.payloadStrideBytes != operation.payloadStrideBytes)
        return base::Result<FrozenVerifiedSparseFamilyCandidateV1, std::string>::Failure("Sparse Family Resource role or layout mismatch.");
    const auto identity = BindingIdentity(verified, binding);
    return base::Result<FrozenVerifiedSparseFamilyCandidateV1, std::string>::Success(
        FrozenVerifiedSparseFamilyCandidateV1(verified, artifact, binding, identity));
}

base::Result<void, std::string> ValidateFrozenVerifiedSparseFamilyCandidateV1(
    const FrozenVerifiedSparseFamilyCandidateV1& frozen,
    const semantic::SparsePointTransformSemanticV1& semanticValue,
    const semantic::ExactSparseWorkSetV1& set,
    const fv::SparseFamilyVerificationContextV1& context,
    std::uint64_t currentDeviceEpoch)
{
    if (currentDeviceEpoch == 0 || currentDeviceEpoch != frozen.ResourceBinding().deviceEpoch)
        return base::Result<void, std::string>::Failure("frozen Sparse Family Candidate belongs to stale or different device epoch");
    if (frozen.BindingIdentity() != BindingIdentity(frozen.Verified(), frozen.ResourceBinding()))
        return base::Result<void, std::string>::Failure("frozen Sparse Family binding identity mismatch");
    return fv::ValidateVerifiedSparseFamilyCandidateV1(frozen.Verified(), semanticValue, set, context);
}

base::Result<SparseCandidateFamilyCaseResultV1, SparseFamilyExecutionErrorV1>
RunVerifiedSparseCandidateFamilyOnWarpV1(
    const semantic::SparsePointTransformSemanticV1& semanticValue,
    semantic::SparsePatternKindV1 pattern,
    const semantic::ExactSparseWorkSetV1& set,
    const fv::SparseFamilyVerificationContextV1& context,
    const std::vector<FrozenVerifiedSparseFamilyCandidateV1>& frozen,
    std::uint64_t currentDeviceEpoch)
{
#if !defined(_WIN32)
    (void)semanticValue; (void)pattern; (void)set; (void)context; (void)frozen; (void)currentDeviceEpoch;
    return base::Result<SparseCandidateFamilyCaseResultV1, SparseFamilyExecutionErrorV1>::Failure(
        Error("execution/platform", "Spiral 6 Sparse Candidate Family requires Windows D3D12."));
#else
    try
    {
        GpuContext gpu;
        auto result = RunVerifiedSparseCandidateFamilyWithGpuV1(
            gpu, semanticValue, pattern, set, context, frozen, currentDeviceEpoch);
        return base::Result<SparseCandidateFamilyCaseResultV1, SparseFamilyExecutionErrorV1>::Success(
            std::move(result));
    }
    catch (const std::exception& error)
    {
        return base::Result<SparseCandidateFamilyCaseResultV1, SparseFamilyExecutionErrorV1>::Failure(
            Error("execution/d3d12", error.what()));
    }
#endif
}

std::vector<std::byte> SerializeFrozenVerifiedSparseFamilyCandidateV1(
    const FrozenVerifiedSparseFamilyCandidateV1& value)
{
    base::BinaryWriter writer;
    Digest(writer, Literal("SGE4-5.Spiral6.FrozenVerifiedSparseFamilyCandidate.V1"));
    const auto verified = fv::SerializeVerifiedSparseFamilyCandidateV1(value.Verified());
    writer.WriteU32(static_cast<std::uint32_t>(verified.size()));
    writer.WriteBytes(verified);
    writer.WriteU32(static_cast<std::uint32_t>(value.Artifact().Bytes().size()));
    writer.WriteBytes(value.Artifact().Bytes());
    writer.WriteU64(value.ResourceBinding().deviceEpoch);
    Digest(writer, value.ResourceBinding().actualRepresentationResourceIdentity);
    writer.WriteU32(static_cast<std::uint32_t>(value.ResourceBinding().representationRole));
    writer.WriteU32(value.ResourceBinding().fixedCapacityBytes);
    writer.WriteU32(value.ResourceBinding().payloadStrideBytes);
    Digest(writer, value.BindingIdentity());
    return std::move(writer).Take();
}

std::vector<std::byte> SerializeSparseCandidateFamilyCaseResultV1(
    const SparseCandidateFamilyCaseResultV1& value)
{
    base::BinaryWriter writer;
    Digest(writer, Literal("SGE4-5.Spiral6.CU4.SparseCandidateFamilyCaseEvidence.V1"));
    writer.WriteU32(static_cast<std::uint32_t>(value.pattern));
    Digest(writer, value.semanticIdentity);
    Digest(writer, value.sparseSetIdentity);
    writer.WriteU32(static_cast<std::uint32_t>(value.candidates.size()));
    for (const auto& candidate : value.candidates)
    {
        writer.WriteU32(static_cast<std::uint32_t>(candidate.kind));
        writer.WriteU32(candidate.activeCount);
        writer.WriteU32(candidate.activeBlockCount);
        writer.WriteU32(candidate.expectedDispatchGroupsX);
        writer.WriteU32(candidate.observedDispatchGroupsX);
        writer.WriteU32(candidate.activeWriteCount);
        writer.WriteU32(candidate.activeReferenceQualified ? 1u : 0u);
        writer.WriteU32(candidate.inactiveSentinelByteStable ? 1u : 0u);
        writer.WriteU32(candidate.activeHomogeneousWQualified ? 1u : 0u);
        writer.WriteU32(candidate.finiteQualified ? 1u : 0u);
        writer.WriteU64(std::bit_cast<std::uint64_t>(candidate.maxAbsoluteError));
        Digest(writer, candidate.artifactIdentity);
        Digest(writer, candidate.bindingIdentity);
        Digest(writer, candidate.outputDigest);
    }
    writer.WriteU32(value.pairwiseOutputByteIdentical ? 1u : 0u);
    Digest(writer, base::Sha256(writer.Bytes()));
    return std::move(writer).Take();
}

namespace
{
SparseCandidateFamilyRuntimeErrorV1 RuntimeError(
    std::string stage,
    std::string message,
    long hresult = 0)
{
    return {std::move(stage), std::move(message), hresult};
}

[[maybe_unused]] base::Digest256 BuildSparseRuntimeCandidateBindingSetIdentityV1(
    const semantic::SparsePointTransformSemanticV1& semanticValue,
    const fv::SparseFamilyVerificationContextV1& context,
    std::span<const SparseSetCandidateAuthorityV1> authorities)
{
    base::BinaryWriter writer;
    Digest(writer, Literal("SGE4-5.Spiral6.SparseCandidateFamilyRuntimeAuthoritySet.V1"));
    const auto semanticBytes = semantic::SerializeSparsePointTransformSemanticV1(semanticValue);
    writer.WriteU32(static_cast<std::uint32_t>(semanticBytes.size()));
    writer.WriteBytes(semanticBytes);
    Digest(writer, context.targetProfileIdentity);
    Digest(writer, context.observationContractIdentity);
    Digest(writer, context.completionContractIdentity);
    Digest(writer, context.deviceEpochPolicyIdentity);
    Digest(writer, context.spiral4IndirectArtifactIdentity);
    writer.WriteU32(static_cast<std::uint32_t>(authorities.size()));
    for (const auto& authority : authorities)
    {
        writer.WriteU32(static_cast<std::uint32_t>(authority.pattern));
        const auto setBytes = semantic::SerializeExactSparseWorkSetV1(authority.set);
        writer.WriteU32(static_cast<std::uint32_t>(setBytes.size()));
        writer.WriteBytes(setBytes);
        writer.WriteU32(static_cast<std::uint32_t>(authority.candidates.size()));
        for (const auto& candidate : authority.candidates)
        {
            const auto verified = fv::SerializeVerifiedSparseFamilyCandidateV1(candidate);
            writer.WriteU32(static_cast<std::uint32_t>(verified.size()));
            writer.WriteBytes(verified);
        }
    }
    return base::Sha256(writer.Bytes());
}

[[maybe_unused]] base::Digest256 BuildSparseRuntimeDomainIdentityV1(
    const semantic::SparsePointTransformSemanticV1& semanticValue,
    const base::Digest256& bindingSetIdentity)
{
    base::BinaryWriter writer;
    Digest(writer, Literal("SGE4-5.Spiral6.LoadedSparseCandidateFamilyRuntime.V1"));
    Digest(writer, semantic::SparsePointTransformSemanticIdentityV1(semanticValue));
    Digest(writer, bindingSetIdentity);
    return base::Sha256(writer.Bytes());
}

const SparseSetCandidateAuthorityV1* FindSparseAuthorityV1(
    std::span<const SparseSetCandidateAuthorityV1> authorities,
    semantic::SparsePatternKindV1 pattern,
    const base::Digest256& setIdentity)
{
    const auto found = std::find_if(
        authorities.begin(), authorities.end(),
        [&](const SparseSetCandidateAuthorityV1& value)
        {
            return value.pattern == pattern && value.set.Identity() == setIdentity;
        });
    return found == authorities.end() ? nullptr : &*found;
}

base::Digest256 BuildSparseSetBindingIdentityV1(
    const base::Digest256& domainIdentity,
    std::uint64_t deviceEpoch,
    semantic::SparsePatternKindV1 pattern,
    const base::Digest256& setIdentity)
{
    base::BinaryWriter writer;
    Digest(writer, Literal("SGE4-5.Spiral6.SparseRuntimeSetBinding.V1"));
    Digest(writer, domainIdentity);
    writer.WriteU64(deviceEpoch);
    writer.WriteU32(static_cast<std::uint32_t>(pattern));
    Digest(writer, setIdentity);
    return base::Sha256(writer.Bytes());
}

base::Digest256 BuildSparseRepresentationSetIdentityV1(
    const SparseRuntimeSetBindingV1& binding,
    std::span<const FrozenVerifiedSparseFamilyCandidateV1> frozen)
{
    base::BinaryWriter writer;
    Digest(writer, Literal("SGE4-5.Spiral6.SparseRuntimeRepresentationSet.V1"));
    writer.WriteU64(binding.deviceEpoch);
    writer.WriteU32(static_cast<std::uint32_t>(binding.pattern));
    Digest(writer, binding.sparseSetIdentity);
    Digest(writer, binding.bindingIdentity);
    writer.WriteU32(static_cast<std::uint32_t>(frozen.size()));
    for (const auto& candidate : frozen)
    {
        const auto bytes = SerializeFrozenVerifiedSparseFamilyCandidateV1(candidate);
        writer.WriteU32(static_cast<std::uint32_t>(bytes.size()));
        writer.WriteBytes(bytes);
    }
    return base::Sha256(writer.Bytes());
}

[[maybe_unused]] base::Digest256 BuildSparseSubmissionTokenIdentityV1(
    const base::Digest256& domainIdentity,
    std::uint64_t deviceEpoch,
    std::uint64_t ordinal,
    const SparseRepresentationSetHandleV1& representations,
    const SparseCandidateFamilyCaseResultV1& architecture)
{
    base::BinaryWriter writer;
    Digest(writer, Literal("SGE4-5.Spiral6.SparseRuntimeSubmissionToken.V1"));
    Digest(writer, domainIdentity);
    writer.WriteU64(deviceEpoch);
    writer.WriteU64(ordinal);
    Digest(writer, representations.representationSetIdentity);
    const auto evidence = SerializeSparseCandidateFamilyCaseResultV1(architecture);
    writer.WriteU32(static_cast<std::uint32_t>(evidence.size()));
    writer.WriteBytes(evidence);
    return base::Sha256(writer.Bytes());
}
}

struct LoadedSparseCandidateFamilyRuntimeV1::Impl final
{
    semantic::SparsePointTransformSemanticV1 semantic;
    fv::SparseFamilyVerificationContextV1 context;
    std::vector<SparseSetCandidateAuthorityV1> authorities;
    SparseCandidateFamilyRuntimeStateV1 state = SparseCandidateFamilyRuntimeStateV1::Active;
    std::uint64_t deviceEpoch = 1;
    std::uint64_t submissionOrdinal = 0;
    base::Digest256 domainIdentity{};
    base::Digest256 candidateBindingSetIdentity{};
    std::string adapterDescription;
    bool sparseSetBound = false;
    bool representationsBuilt = false;
    SparseRuntimeSetBindingV1 currentBinding{};
    SparseRepresentationSetHandleV1 currentRepresentations{};
    std::vector<FrozenVerifiedSparseFamilyCandidateV1> currentFrozen;
    std::vector<SparseRuntimeSubmissionTokenV1> issuedTokens;
    std::vector<SparseRepresentationSetHandleV1> issuedRepresentations;
#if defined(_WIN32)
    std::unique_ptr<GpuContext> gpu;
    bool hasExcludedAdapterLuid = false;
    LUID excludedAdapterLuid{};
#endif
};

LoadedSparseCandidateFamilyRuntimeV1::LoadedSparseCandidateFamilyRuntimeV1(
    std::unique_ptr<Impl> impl)
    : impl_(std::move(impl))
{
}
LoadedSparseCandidateFamilyRuntimeV1::LoadedSparseCandidateFamilyRuntimeV1(
    LoadedSparseCandidateFamilyRuntimeV1&&) noexcept = default;
LoadedSparseCandidateFamilyRuntimeV1& LoadedSparseCandidateFamilyRuntimeV1::operator=(
    LoadedSparseCandidateFamilyRuntimeV1&&) noexcept = default;
LoadedSparseCandidateFamilyRuntimeV1::~LoadedSparseCandidateFamilyRuntimeV1() = default;

SparseCandidateFamilyRuntimeStateV1 LoadedSparseCandidateFamilyRuntimeV1::State() const noexcept
{
    return impl_->state;
}
std::uint64_t LoadedSparseCandidateFamilyRuntimeV1::DeviceEpoch() const noexcept
{
    return impl_->deviceEpoch;
}
const base::Digest256& LoadedSparseCandidateFamilyRuntimeV1::DomainIdentity() const noexcept
{
    return impl_->domainIdentity;
}
const base::Digest256& LoadedSparseCandidateFamilyRuntimeV1::CandidateBindingSetIdentity() const noexcept
{
    return impl_->candidateBindingSetIdentity;
}
const std::string& LoadedSparseCandidateFamilyRuntimeV1::AdapterDescription() const noexcept
{
    return impl_->adapterDescription;
}
std::size_t LoadedSparseCandidateFamilyRuntimeV1::AuthorityCount() const noexcept
{
    return impl_->authorities.size();
}
bool LoadedSparseCandidateFamilyRuntimeV1::SparseSetBound() const noexcept
{
    return impl_->sparseSetBound;
}
bool LoadedSparseCandidateFamilyRuntimeV1::RepresentationsBuilt() const noexcept
{
    return impl_->representationsBuilt;
}

base::Result<LoadedSparseCandidateFamilyRuntimeV1, SparseCandidateFamilyRuntimeErrorV1>
LoadSparseCandidateFamilyRuntimeOnWarpV1(
    const semantic::SparsePointTransformSemanticV1& semanticValue,
    const fv::SparseFamilyVerificationContextV1& context,
    std::span<const SparseSetCandidateAuthorityV1> authorities)
{
#if !defined(_WIN32)
    (void)semanticValue; (void)context; (void)authorities;
    return base::Result<LoadedSparseCandidateFamilyRuntimeV1, SparseCandidateFamilyRuntimeErrorV1>::Failure(
        RuntimeError("sparse-runtime/platform", "Spiral 6 CU5 Runtime requires Windows D3D12."));
#else
    try
    {
        if (authorities.empty())
            throw std::runtime_error("Sparse Runtime authority corpus is empty.");
        std::set<std::pair<std::uint32_t, std::string>> unique;
        for (const auto& authority : authorities)
        {
            if (authority.candidates.size() != 3)
                throw std::runtime_error("Sparse Runtime authority requires three Candidates per set.");
            const auto key = std::pair{
                static_cast<std::uint32_t>(authority.pattern),
                base::ToHex(authority.set.Identity())};
            if (!unique.insert(key).second)
                throw std::runtime_error("Sparse Runtime authority contains a duplicate pattern/set pair.");
            std::set<fc::SparseFamilyCandidateKindV1> kinds;
            for (const auto& candidate : authority.candidates)
            {
                auto valid = fv::ValidateVerifiedSparseFamilyCandidateV1(
                    candidate, semanticValue, authority.set, context);
                if (!valid) throw std::runtime_error(valid.Error());
                kinds.insert(candidate.Plan().operation.kind);
            }
            if (kinds.size() != 3)
                throw std::runtime_error("Sparse Runtime authority does not contain three unique Candidate kinds.");
        }

        auto impl = std::make_unique<LoadedSparseCandidateFamilyRuntimeV1::Impl>();
        impl->semantic = semanticValue;
        impl->context = context;
        impl->authorities.assign(authorities.begin(), authorities.end());
        impl->candidateBindingSetIdentity = BuildSparseRuntimeCandidateBindingSetIdentityV1(
            semanticValue, context, authorities);
        impl->domainIdentity = BuildSparseRuntimeDomainIdentityV1(
            semanticValue, impl->candidateBindingSetIdentity);
        impl->gpu = std::make_unique<GpuContext>();
        impl->adapterDescription = impl->gpu->adapterDescription;
        return base::Result<LoadedSparseCandidateFamilyRuntimeV1, SparseCandidateFamilyRuntimeErrorV1>::Success(
            LoadedSparseCandidateFamilyRuntimeV1(std::move(impl)));
    }
    catch (const std::exception& error)
    {
        return base::Result<LoadedSparseCandidateFamilyRuntimeV1, SparseCandidateFamilyRuntimeErrorV1>::Failure(
            RuntimeError("sparse-runtime/load", error.what()));
    }
#endif
}

SparseRuntimeEpochHandleV1 CaptureSparseRuntimeEpochHandleV1(
    const LoadedSparseCandidateFamilyRuntimeV1& runtime)
{
    SparseRuntimeEpochHandleV1 handle;
    handle.deviceEpoch = runtime.impl_->deviceEpoch;
    handle.domainIdentity = runtime.impl_->domainIdentity;
    handle.candidateBindingSetIdentity = runtime.impl_->candidateBindingSetIdentity;
    return handle;
}

base::Result<void, SparseCandidateFamilyRuntimeErrorV1>
ValidateSparseRuntimeEpochHandleV1(
    const LoadedSparseCandidateFamilyRuntimeV1& runtime,
    const SparseRuntimeEpochHandleV1& handle)
{
    const auto& impl = *runtime.impl_;
    if (impl.state != SparseCandidateFamilyRuntimeStateV1::Active)
        return base::Result<void, SparseCandidateFamilyRuntimeErrorV1>::Failure(
            RuntimeError("sparse-runtime/device-state", "Sparse Runtime domain is not Active."));
    if (handle.deviceEpoch != impl.deviceEpoch)
        return base::Result<void, SparseCandidateFamilyRuntimeErrorV1>::Failure(
            RuntimeError("sparse-runtime/stale-epoch", "Sparse Runtime handle belongs to another device epoch."));
    if (handle.domainIdentity != impl.domainIdentity ||
        handle.candidateBindingSetIdentity != impl.candidateBindingSetIdentity)
        return base::Result<void, SparseCandidateFamilyRuntimeErrorV1>::Failure(
            RuntimeError("sparse-runtime/handle-identity", "Sparse Runtime handle identity mismatch."));
    return base::Result<void, SparseCandidateFamilyRuntimeErrorV1>::Success();
}

base::Result<SparseRuntimeSetBindingV1, SparseCandidateFamilyRuntimeErrorV1>
BindSparseWorkSetV1(
    LoadedSparseCandidateFamilyRuntimeV1& runtime,
    const SparseRuntimeEpochHandleV1& handle,
    semantic::SparsePatternKindV1 pattern,
    const base::Digest256& sparseSetIdentity)
{
    auto handleValid = ValidateSparseRuntimeEpochHandleV1(runtime, handle);
    if (!handleValid)
        return base::Result<SparseRuntimeSetBindingV1, SparseCandidateFamilyRuntimeErrorV1>::Failure(
            handleValid.Error());
    auto& impl = *runtime.impl_;
    if (!FindSparseAuthorityV1(impl.authorities, pattern, sparseSetIdentity))
        return base::Result<SparseRuntimeSetBindingV1, SparseCandidateFamilyRuntimeErrorV1>::Failure(
            RuntimeError("sparse-runtime/set-authority", "Sparse set is not present in the Frozen authority corpus."));

    SparseRuntimeSetBindingV1 binding;
    binding.deviceEpoch = impl.deviceEpoch;
    binding.pattern = pattern;
    binding.sparseSetIdentity = sparseSetIdentity;
    binding.bindingIdentity = BuildSparseSetBindingIdentityV1(
        impl.domainIdentity, impl.deviceEpoch, pattern, sparseSetIdentity);
    impl.sparseSetBound = true;
    impl.representationsBuilt = false;
    impl.currentBinding = binding;
    impl.currentRepresentations = {};
    impl.currentFrozen.clear();
    return base::Result<SparseRuntimeSetBindingV1, SparseCandidateFamilyRuntimeErrorV1>::Success(binding);
}

base::Result<SparseRepresentationSetHandleV1, SparseCandidateFamilyRuntimeErrorV1>
RebuildSparseRepresentationsV1(
    LoadedSparseCandidateFamilyRuntimeV1& runtime,
    const SparseRuntimeEpochHandleV1& handle,
    const SparseRuntimeSetBindingV1& binding)
{
    auto handleValid = ValidateSparseRuntimeEpochHandleV1(runtime, handle);
    if (!handleValid)
        return base::Result<SparseRepresentationSetHandleV1, SparseCandidateFamilyRuntimeErrorV1>::Failure(
            handleValid.Error());
    auto& impl = *runtime.impl_;
    if (!impl.sparseSetBound)
        return base::Result<SparseRepresentationSetHandleV1, SparseCandidateFamilyRuntimeErrorV1>::Failure(
            RuntimeError("sparse-runtime/rebind-required", "Current-epoch Sparse set must be rebound before representation rebuild."));
    if (binding.deviceEpoch != impl.deviceEpoch ||
        binding.pattern != impl.currentBinding.pattern ||
        binding.sparseSetIdentity != impl.currentBinding.sparseSetIdentity ||
        binding.bindingIdentity != impl.currentBinding.bindingIdentity ||
        binding.bindingIdentity != BuildSparseSetBindingIdentityV1(
            impl.domainIdentity, binding.deviceEpoch, binding.pattern, binding.sparseSetIdentity))
        return base::Result<SparseRepresentationSetHandleV1, SparseCandidateFamilyRuntimeErrorV1>::Failure(
            RuntimeError("sparse-runtime/binding-identity", "Sparse set binding does not match the current Runtime binding."));

    const auto* authority = FindSparseAuthorityV1(
        impl.authorities, binding.pattern, binding.sparseSetIdentity);
    if (!authority)
        return base::Result<SparseRepresentationSetHandleV1, SparseCandidateFamilyRuntimeErrorV1>::Failure(
            RuntimeError("sparse-runtime/set-authority", "Sparse set authority disappeared during rebuild."));

    try
    {
        std::vector<FrozenVerifiedSparseFamilyCandidateV1> frozen;
        for (const auto& verified : authority->candidates)
        {
            const auto kind = verified.Plan().operation.kind;
            const auto artifact = fc::BuildSparseFamilyArtifactV1(
                impl.semantic, authority->set, kind);
            const auto resource = BuildCanonicalSparseFamilyResourceBindingInputV1(
                verified, impl.deviceEpoch);
            auto value = FreezeVerifiedSparseFamilyCandidateV1(
                impl.semantic, authority->set, impl.context, verified, artifact, resource);
            if (!value) throw std::runtime_error(value.Error());
            frozen.push_back(std::move(value).Value());
        }
        SparseRepresentationSetHandleV1 representations;
        representations.deviceEpoch = impl.deviceEpoch;
        representations.pattern = binding.pattern;
        representations.sparseSetIdentity = binding.sparseSetIdentity;
        representations.representationSetIdentity = BuildSparseRepresentationSetIdentityV1(
            binding, frozen);
        impl.currentFrozen = std::move(frozen);
        impl.currentRepresentations = representations;
        impl.representationsBuilt = true;
        impl.issuedRepresentations.push_back(representations);
        return base::Result<SparseRepresentationSetHandleV1, SparseCandidateFamilyRuntimeErrorV1>::Success(
            representations);
    }
    catch (const std::exception& error)
    {
        return base::Result<SparseRepresentationSetHandleV1, SparseCandidateFamilyRuntimeErrorV1>::Failure(
            RuntimeError("sparse-runtime/rebuild", error.what()));
    }
}

base::Result<void, SparseCandidateFamilyRuntimeErrorV1>
ValidateSparseRepresentationSetHandleV1(
    const LoadedSparseCandidateFamilyRuntimeV1& runtime,
    const SparseRepresentationSetHandleV1& representations)
{
    const auto& impl = *runtime.impl_;
    if (impl.state != SparseCandidateFamilyRuntimeStateV1::Active)
        return base::Result<void, SparseCandidateFamilyRuntimeErrorV1>::Failure(
            RuntimeError("sparse-runtime/device-state", "Sparse representation belongs to an inactive domain."));
    if (representations.deviceEpoch != impl.deviceEpoch)
        return base::Result<void, SparseCandidateFamilyRuntimeErrorV1>::Failure(
            RuntimeError("sparse-runtime/stale-epoch", "Sparse representation belongs to another device epoch."));
    const bool issued = std::any_of(
        impl.issuedRepresentations.begin(), impl.issuedRepresentations.end(),
        [&](const SparseRepresentationSetHandleV1& value)
        {
            return value.deviceEpoch == representations.deviceEpoch &&
                value.pattern == representations.pattern &&
                value.sparseSetIdentity == representations.sparseSetIdentity &&
                value.representationSetIdentity == representations.representationSetIdentity;
        });
    if (!issued)
        return base::Result<void, SparseCandidateFamilyRuntimeErrorV1>::Failure(
            RuntimeError("sparse-runtime/representation-identity", "Sparse representation handle is not recognized by this Runtime."));
    return base::Result<void, SparseCandidateFamilyRuntimeErrorV1>::Success();
}

base::Result<SparseRuntimeSubmissionV1, SparseCandidateFamilyRuntimeErrorV1>
SubmitSparseWorkSetV1(
    LoadedSparseCandidateFamilyRuntimeV1& runtime,
    const SparseRuntimeEpochHandleV1& handle,
    const SparseRuntimeSetBindingV1& binding,
    const SparseRepresentationSetHandleV1& representations)
{
#if !defined(_WIN32)
    (void)runtime; (void)handle; (void)binding; (void)representations;
    return base::Result<SparseRuntimeSubmissionV1, SparseCandidateFamilyRuntimeErrorV1>::Failure(
        RuntimeError("sparse-runtime/platform", "Spiral 6 CU5 submission requires Windows D3D12."));
#else
    auto handleValid = ValidateSparseRuntimeEpochHandleV1(runtime, handle);
    if (!handleValid)
        return base::Result<SparseRuntimeSubmissionV1, SparseCandidateFamilyRuntimeErrorV1>::Failure(
            handleValid.Error());
    auto& impl = *runtime.impl_;
    if (!impl.sparseSetBound)
        return base::Result<SparseRuntimeSubmissionV1, SparseCandidateFamilyRuntimeErrorV1>::Failure(
            RuntimeError("sparse-runtime/rebind-required", "Current-epoch Sparse set must be rebound before submission."));
    if (binding.deviceEpoch != impl.deviceEpoch || binding.bindingIdentity != impl.currentBinding.bindingIdentity)
        return base::Result<SparseRuntimeSubmissionV1, SparseCandidateFamilyRuntimeErrorV1>::Failure(
            RuntimeError("sparse-runtime/binding-identity", "Sparse submission binding is not current."));
    if (!impl.representationsBuilt)
        return base::Result<SparseRuntimeSubmissionV1, SparseCandidateFamilyRuntimeErrorV1>::Failure(
            RuntimeError("sparse-runtime/rebuild-required", "Sparse derived representations must be explicitly rebuilt before submission."));
    auto representationValid = ValidateSparseRepresentationSetHandleV1(runtime, representations);
    if (!representationValid)
        return base::Result<SparseRuntimeSubmissionV1, SparseCandidateFamilyRuntimeErrorV1>::Failure(
            representationValid.Error());
    if (representations.representationSetIdentity != impl.currentRepresentations.representationSetIdentity)
        return base::Result<SparseRuntimeSubmissionV1, SparseCandidateFamilyRuntimeErrorV1>::Failure(
            RuntimeError("sparse-runtime/representation-current", "Sparse representation handle is not the current set binding."));
    const auto* authority = FindSparseAuthorityV1(
        impl.authorities, binding.pattern, binding.sparseSetIdentity);
    if (!authority)
        return base::Result<SparseRuntimeSubmissionV1, SparseCandidateFamilyRuntimeErrorV1>::Failure(
            RuntimeError("sparse-runtime/set-authority", "Sparse submission authority is missing."));
    try
    {
        auto architecture = RunVerifiedSparseCandidateFamilyWithGpuV1(
            *impl.gpu, impl.semantic, authority->pattern, authority->set,
            impl.context, impl.currentFrozen, impl.deviceEpoch);
        SparseRuntimeSubmissionV1 submission;
        submission.representations = representations;
        submission.representationResourcesMaterialized = true;
        submission.indirectArgumentsMaterialized = true;
        submission.completionStateMaterialized = true;
        submission.readbackStateMaterialized = true;
        submission.architecture = std::move(architecture);
        submission.completion.deviceEpoch = impl.deviceEpoch;
        submission.completion.submissionOrdinal = ++impl.submissionOrdinal;
        submission.completion.tokenIdentity = BuildSparseSubmissionTokenIdentityV1(
            impl.domainIdentity, impl.deviceEpoch, submission.completion.submissionOrdinal,
            representations, submission.architecture);
        impl.issuedTokens.push_back(submission.completion);
        return base::Result<SparseRuntimeSubmissionV1, SparseCandidateFamilyRuntimeErrorV1>::Success(
            std::move(submission));
    }
    catch (const std::exception& error)
    {
        return base::Result<SparseRuntimeSubmissionV1, SparseCandidateFamilyRuntimeErrorV1>::Failure(
            RuntimeError("sparse-runtime/submit", error.what()));
    }
#endif
}

base::Result<void, SparseCandidateFamilyRuntimeErrorV1>
ValidateSparseRuntimeSubmissionTokenV1(
    const LoadedSparseCandidateFamilyRuntimeV1& runtime,
    const SparseRuntimeSubmissionTokenV1& token)
{
    const auto& impl = *runtime.impl_;
    if (impl.state != SparseCandidateFamilyRuntimeStateV1::Active)
        return base::Result<void, SparseCandidateFamilyRuntimeErrorV1>::Failure(
            RuntimeError("sparse-runtime/device-state", "Sparse submission belongs to an inactive domain."));
    if (token.deviceEpoch != impl.deviceEpoch)
        return base::Result<void, SparseCandidateFamilyRuntimeErrorV1>::Failure(
            RuntimeError("sparse-runtime/stale-epoch", "Sparse submission token belongs to another device epoch."));
    const bool issued = std::any_of(
        impl.issuedTokens.begin(), impl.issuedTokens.end(),
        [&](const SparseRuntimeSubmissionTokenV1& value)
        {
            return value.deviceEpoch == token.deviceEpoch &&
                value.submissionOrdinal == token.submissionOrdinal &&
                value.tokenIdentity == token.tokenIdentity;
        });
    if (!issued)
        return base::Result<void, SparseCandidateFamilyRuntimeErrorV1>::Failure(
            RuntimeError("sparse-runtime/token-identity", "Sparse submission token is not recognized by this Runtime."));
    return base::Result<void, SparseCandidateFamilyRuntimeErrorV1>::Success();
}

base::Result<SparseCandidateFamilyRecoveryReportV1, SparseCandidateFamilyRuntimeErrorV1>
RecoverSparseCandidateFamilyRuntimeV1(
    LoadedSparseCandidateFamilyRuntimeV1& runtime,
    SparseCandidateFamilyRecoveryModeV1 mode)
{
#if !defined(_WIN32)
    (void)runtime; (void)mode;
    return base::Result<SparseCandidateFamilyRecoveryReportV1, SparseCandidateFamilyRuntimeErrorV1>::Failure(
        RuntimeError("sparse-runtime/platform", "Spiral 6 CU5 Recovery requires Windows D3D12."));
#else
    auto& impl = *runtime.impl_;
    SparseCandidateFamilyRecoveryReportV1 report;
    report.mode = mode;
    report.stateBefore = impl.state;
    report.stateAfter = impl.state;
    report.previousDeviceEpoch = impl.deviceEpoch;
    report.newDeviceEpoch = impl.deviceEpoch;
    const auto frozenBefore = impl.candidateBindingSetIdentity;

    if (mode == SparseCandidateFamilyRecoveryModeV1::RetryAdapterReacquisition)
    {
        if (impl.state != SparseCandidateFamilyRuntimeStateV1::AwaitingAdapter ||
            !impl.hasExcludedAdapterLuid)
            return base::Result<SparseCandidateFamilyRecoveryReportV1, SparseCandidateFamilyRuntimeErrorV1>::Failure(
                RuntimeError("sparse-runtime/retry-state", "Sparse adapter retry requires an excluded removed adapter."));
        try
        {
            ComPtr<IDXGIFactory6> factory;
            Check(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)), "CreateDXGIFactory2(retry)");
            ComPtr<IDXGIAdapter1> warp;
            Check(factory->EnumWarpAdapter(IID_PPV_ARGS(&warp)), "EnumWarpAdapter(retry)");
            DXGI_ADAPTER_DESC1 description{};
            Check(warp->GetDesc1(&description), "GetDesc1(retry)");
            if (description.AdapterLuid.LowPart == impl.excludedAdapterLuid.LowPart &&
                description.AdapterLuid.HighPart == impl.excludedAdapterLuid.HighPart)
            {
                report.adapterReacquired = false;
                report.sparseRebindRequired = true;
                report.explicitRepresentationRebuildRequired = true;
                report.frozenAuthorityPreserved = frozenBefore == impl.candidateBindingSetIdentity;
                return base::Result<SparseCandidateFamilyRecoveryReportV1, SparseCandidateFamilyRuntimeErrorV1>::Success(report);
            }
            impl.gpu = std::make_unique<GpuContext>();
            ++impl.deviceEpoch;
            impl.state = SparseCandidateFamilyRuntimeStateV1::Active;
            impl.adapterDescription = impl.gpu->adapterDescription;
            impl.sparseSetBound = false;
            impl.representationsBuilt = false;
            report.adapterReacquired = true;
            report.nativeDeviceRematerialized = true;
            report.executionContextRematerialized = true;
            report.newDeviceEpoch = impl.deviceEpoch;
            report.stateAfter = impl.state;
            report.sparseRebindRequired = true;
            report.explicitRepresentationRebuildRequired = true;
            report.frozenAuthorityPreserved = frozenBefore == impl.candidateBindingSetIdentity;
            return base::Result<SparseCandidateFamilyRecoveryReportV1, SparseCandidateFamilyRuntimeErrorV1>::Success(report);
        }
        catch (const std::exception& error)
        {
            return base::Result<SparseCandidateFamilyRecoveryReportV1, SparseCandidateFamilyRuntimeErrorV1>::Failure(
                RuntimeError("sparse-runtime/retry", error.what()));
        }
    }

    if (impl.state != SparseCandidateFamilyRuntimeStateV1::Active || !impl.gpu)
        return base::Result<SparseCandidateFamilyRecoveryReportV1, SparseCandidateFamilyRuntimeErrorV1>::Failure(
            RuntimeError("sparse-runtime/device-state", "Recovery requires an active Sparse Runtime."));

    report.representationResourcesInvalidated = true;
    report.indirectArgumentsInvalidated = true;
    report.completionStateInvalidated = true;
    report.readbackStateInvalidated = true;
    report.sparseRebindRequired = true;
    report.explicitRepresentationRebuildRequired = true;

    if (mode == SparseCandidateFamilyRecoveryModeV1::ControlledRebuild)
    {
        try
        {
            impl.gpu.reset();
            report.allRuntimeObjectsReleased = true;
            impl.gpu = std::make_unique<GpuContext>();
            impl.adapterDescription = impl.gpu->adapterDescription;
            ++impl.deviceEpoch;
            impl.submissionOrdinal = 0;
            impl.sparseSetBound = false;
            impl.representationsBuilt = false;
            impl.currentBinding = {};
            impl.currentRepresentations = {};
            impl.currentFrozen.clear();
            impl.issuedTokens.clear();
            impl.issuedRepresentations.clear();
            report.adapterReacquired = true;
            report.nativeDeviceRematerialized = true;
            report.executionContextRematerialized = true;
            report.newDeviceEpoch = impl.deviceEpoch;
            report.stateAfter = impl.state;
            report.frozenAuthorityPreserved = frozenBefore == impl.candidateBindingSetIdentity;
            return base::Result<SparseCandidateFamilyRecoveryReportV1, SparseCandidateFamilyRuntimeErrorV1>::Success(report);
        }
        catch (const std::exception& error)
        {
            return base::Result<SparseCandidateFamilyRecoveryReportV1, SparseCandidateFamilyRuntimeErrorV1>::Failure(
                RuntimeError("sparse-runtime/controlled", error.what()));
        }
    }

    if (mode != SparseCandidateFamilyRecoveryModeV1::ForceRemovalForTest)
        return base::Result<SparseCandidateFamilyRecoveryReportV1, SparseCandidateFamilyRuntimeErrorV1>::Failure(
            RuntimeError("sparse-runtime/recovery-mode", "Unknown Sparse Recovery mode."));

    ComPtr<ID3D12Device5> removable;
    HRESULT hr = impl.gpu->device.As(&removable);
    if (FAILED(hr))
        return base::Result<SparseCandidateFamilyRecoveryReportV1, SparseCandidateFamilyRuntimeErrorV1>::Failure(
            RuntimeError("sparse-runtime/query-device5", HResultText(hr), hr));
    removable->RemoveDevice();
    const HRESULT reason = impl.gpu->device->GetDeviceRemovedReason();
    report.removalReason = static_cast<long>(reason);
    if (SUCCEEDED(reason))
        return base::Result<SparseCandidateFamilyRecoveryReportV1, SparseCandidateFamilyRuntimeErrorV1>::Failure(
            RuntimeError("sparse-runtime/remove-device", "ID3D12Device5::RemoveDevice did not remove the Sparse device."));

    report.forcedRemoval = true;
    impl.excludedAdapterLuid = impl.gpu->adapterLuid;
    impl.hasExcludedAdapterLuid = true;
    report.removedAdapterLuidLow = impl.excludedAdapterLuid.LowPart;
    report.removedAdapterLuidHigh = impl.excludedAdapterLuid.HighPart;
    impl.gpu.reset();
    impl.state = SparseCandidateFamilyRuntimeStateV1::AwaitingAdapter;
    impl.sparseSetBound = false;
    impl.representationsBuilt = false;
    impl.currentBinding = {};
    impl.currentRepresentations = {};
    impl.currentFrozen.clear();
    impl.issuedTokens.clear();
    impl.issuedRepresentations.clear();
    report.allRuntimeObjectsReleased = true;
    report.stateAfter = impl.state;
    report.frozenAuthorityPreserved = frozenBefore == impl.candidateBindingSetIdentity;
    return base::Result<SparseCandidateFamilyRecoveryReportV1, SparseCandidateFamilyRuntimeErrorV1>::Success(report);
#endif
}

std::vector<std::byte> SerializeSparseCandidateFamilyRuntimeAuthorityV1(
    const LoadedSparseCandidateFamilyRuntimeV1& runtime)
{
    const auto& impl = *runtime.impl_;
    base::BinaryWriter writer;
    Digest(writer, Literal("SGE4-5.Spiral6.SparseCandidateFamilyRuntimeAuthority.V1"));
    const auto semanticBytes = semantic::SerializeSparsePointTransformSemanticV1(impl.semantic);
    writer.WriteU32(static_cast<std::uint32_t>(semanticBytes.size()));
    writer.WriteBytes(semanticBytes);
    Digest(writer, impl.context.targetProfileIdentity);
    Digest(writer, impl.context.observationContractIdentity);
    Digest(writer, impl.context.completionContractIdentity);
    Digest(writer, impl.context.deviceEpochPolicyIdentity);
    Digest(writer, impl.context.spiral4IndirectArtifactIdentity);
    writer.WriteU32(static_cast<std::uint32_t>(impl.authorities.size()));
    for (const auto& authority : impl.authorities)
    {
        writer.WriteU32(static_cast<std::uint32_t>(authority.pattern));
        const auto setBytes = semantic::SerializeExactSparseWorkSetV1(authority.set);
        writer.WriteU32(static_cast<std::uint32_t>(setBytes.size()));
        writer.WriteBytes(setBytes);
        writer.WriteU32(static_cast<std::uint32_t>(authority.candidates.size()));
        for (const auto& candidate : authority.candidates)
        {
            const auto verified = fv::SerializeVerifiedSparseFamilyCandidateV1(candidate);
            writer.WriteU32(static_cast<std::uint32_t>(verified.size()));
            writer.WriteBytes(verified);
        }
    }
    Digest(writer, impl.domainIdentity);
    Digest(writer, impl.candidateBindingSetIdentity);
    Digest(writer, base::Sha256(writer.Bytes()));
    return std::move(writer).Take();
}

}
