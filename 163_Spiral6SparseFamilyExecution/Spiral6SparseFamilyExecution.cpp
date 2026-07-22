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
        if (frozen.size() != 3)
            return base::Result<SparseCandidateFamilyCaseResultV1, SparseFamilyExecutionErrorV1>::Failure(
                Error("execution/family", "Sparse Candidate Family requires exactly three Frozen Candidates."));
        std::set<fc::SparseFamilyCandidateKindV1> kinds;
        for (const auto& value : frozen)
        {
            auto valid = ValidateFrozenVerifiedSparseFamilyCandidateV1(value, semanticValue, set, context, currentDeviceEpoch);
            if (!valid) throw std::runtime_error(valid.Error());
            kinds.insert(value.Artifact().Kind());
        }
        if (kinds.size() != 3) throw std::runtime_error("Sparse Candidate Family kinds are not unique.");
        GpuContext gpu;
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
        result.pairwiseOutputByteIdentical = outputs[0] == outputs[1] && outputs[1] == outputs[2];
        if (!result.pairwiseOutputByteIdentical)
            throw std::runtime_error("Sparse Candidate Family outputs are not byte-identical.");
        return base::Result<SparseCandidateFamilyCaseResultV1, SparseFamilyExecutionErrorV1>::Success(std::move(result));
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
}
