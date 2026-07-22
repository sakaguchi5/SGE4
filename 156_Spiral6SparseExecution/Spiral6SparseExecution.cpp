#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "Spiral6SparseExecution.h"

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
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>

#if defined(_WIN32)
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib")
#endif

namespace sge4_5::spiral6::execution
{
namespace
{
SparseExecutionErrorV1 Error(std::string stage, std::string message, long nativeCode = 0)
{
    return {std::move(stage), std::move(message), nativeCode};
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

ComPtr<ID3DBlob> CompileShader(std::string_view source)
{
    ComPtr<ID3DBlob> shader;
    ComPtr<ID3DBlob> errors;
    const UINT flags = D3DCOMPILE_ENABLE_STRICTNESS |
                       D3DCOMPILE_WARNINGS_ARE_ERRORS |
                       D3DCOMPILE_OPTIMIZATION_LEVEL3;
    const HRESULT hr = D3DCompile(
        source.data(), source.size(), "Spiral6CompactIndexListV1", nullptr, nullptr,
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

ComPtr<ID3D12Resource> CreateBuffer(
    ID3D12Device* device,
    D3D12_HEAP_TYPE heapType,
    std::uint64_t size,
    D3D12_RESOURCE_STATES state,
    D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE)
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
    Check(device->CreateCommittedResource(
        &heap, D3D12_HEAP_FLAG_NONE, &desc, state, nullptr, IID_PPV_ARGS(&resource)),
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
    D3D12_RANGE readRange{0, size};
    Check(resource->Map(0, &readRange, &mapped), "Map(readback)");
    const auto* first = static_cast<const std::byte*>(mapped);
    std::vector<std::byte> bytes(first, first + size);
    D3D12_RANGE noWrite{0, 0};
    resource->Unmap(0, &noWrite);
    return bytes;
}

D3D12_RESOURCE_BARRIER Transition(
    ID3D12Resource* resource,
    D3D12_RESOURCE_STATES before,
    D3D12_RESOURCE_STATES after)
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

std::string CompactIndexShader()
{
    return R"(
struct Motor { float4 qr; float4 qd; };
cbuffer SparseParameters : register(b0) { uint activeCount; };
StructuredBuffer<uint> SparseIndices : register(t0);
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
[numthreads(64,1,1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint compactOrdinal = dispatchThreadId.x;
    if (compactOrdinal >= activeCount) return;
    uint universeIndex = SparseIndices[compactOrdinal];
    if (universeIndex >= 4096) return;
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
    ComPtr<ID3D12PipelineState> pipeline;
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
        Check(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)),
              "D3D12CreateDevice(WARP)");

        D3D12_COMMAND_QUEUE_DESC queueDesc{};
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        Check(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&queue)), "CreateCommandQueue");
        Check(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator)),
              "CreateCommandAllocator");
        Check(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr,
                                        IID_PPV_ARGS(&commandList)), "CreateCommandList");
        Check(commandList->Close(), "Initial Close");
        Check(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)), "CreateFence");

        std::array<D3D12_ROOT_PARAMETER, 5> parameters{};
        parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        parameters[0].Constants.ShaderRegister = 0;
        parameters[0].Constants.RegisterSpace = 0;
        parameters[0].Constants.Num32BitValues = 1;
        parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        for (std::uint32_t i = 0; i < 3; ++i)
        {
            parameters[i + 1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
            parameters[i + 1].Descriptor.ShaderRegister = i;
            parameters[i + 1].Descriptor.RegisterSpace = 0;
            parameters[i + 1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        }
        parameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
        parameters[4].Descriptor.ShaderRegister = 0;
        parameters[4].Descriptor.RegisterSpace = 0;
        parameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        D3D12_ROOT_SIGNATURE_DESC rootDesc{};
        rootDesc.NumParameters = static_cast<UINT>(parameters.size());
        rootDesc.pParameters = parameters.data();
        rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
        ComPtr<ID3DBlob> serialized;
        ComPtr<ID3DBlob> errors;
        const HRESULT rootResult = D3D12SerializeRootSignature(
            &rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &errors);
        if (FAILED(rootResult))
        {
            const std::string message = errors
                ? std::string(static_cast<const char*>(errors->GetBufferPointer()), errors->GetBufferSize())
                : HResultText(rootResult);
            throw std::runtime_error("D3D12SerializeRootSignature: " + message);
        }
        Check(device->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(),
                                          IID_PPV_ARGS(&rootSignature)), "CreateRootSignature");
        auto shader = CompileShader(CompactIndexShader());
        D3D12_COMPUTE_PIPELINE_STATE_DESC pipelineDesc{};
        pipelineDesc.pRootSignature = rootSignature.Get();
        pipelineDesc.CS = {shader->GetBufferPointer(), shader->GetBufferSize()};
        Check(device->CreateComputePipelineState(&pipelineDesc, IID_PPV_ARGS(&pipeline)),
              "CreateComputePipelineState");

        D3D12_INDIRECT_ARGUMENT_DESC indirectArgument{};
        indirectArgument.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;
        D3D12_COMMAND_SIGNATURE_DESC signatureDesc{};
        signatureDesc.ByteStride = 12;
        signatureDesc.NumArgumentDescs = 1;
        signatureDesc.pArgumentDescs = &indirectArgument;
        Check(device->CreateCommandSignature(&signatureDesc, nullptr, IID_PPV_ARGS(&dispatchSignature)),
              "CreateCommandSignature(Dispatch)");
    }

    void Begin()
    {
        Check(allocator->Reset(), "CommandAllocator::Reset");
        Check(commandList->Reset(allocator.Get(), nullptr), "CommandList::Reset");
    }

    std::uint64_t SubmitAndWait()
    {
        Check(commandList->Close(), "CommandList::Close");
        ID3D12CommandList* lists[]{commandList.Get()};
        queue->ExecuteCommandLists(1, lists);
        ++fenceValue;
        Check(queue->Signal(fence.Get(), fenceValue), "CommandQueue::Signal");
        if (fence->GetCompletedValue() < fenceValue)
        {
            Check(fence->SetEventOnCompletion(fenceValue, fenceEvent.value), "Fence::SetEventOnCompletion");
            WaitForSingleObject(fenceEvent.value, INFINITE);
        }
        return fenceValue;
    }
};

CompactIndexCaseObservationV1 ExecuteCase(
    GpuContext& gpu,
    const semantic::SparsePointTransformSemanticV1& semanticValue,
    const CompactIndexArchitectureCaseV1& testCase)
{
    auto valid = contract::ValidateCompactIndexListArtifactForExecutionV1(
        testCase.artifact, semanticValue, testCase.set);
    if (!valid) throw std::runtime_error(valid.Error().stage + ": " + valid.Error().message);

    const auto motors = semantic::BuildCanonicalGlobalMotorPaletteV1();
    const auto motorBytes = spiral5::semantic::SerializeMotorRecordsV1(motors);
    const auto points = spiral5::semantic::BuildCanonicalPointCorpusV1();
    const auto pointBytes = spiral5::semantic::SerializePointRecordsV1(points);
    const auto zeroBytes = semantic::BuildCanonicalInactiveOutputBytesV1();
    const auto expected = semantic::BuildSparseCpuReferenceOutputV1(testCase.set);
    const auto expectedBytes = spiral5::semantic::SerializePointRecordsV1(expected);

    std::vector<std::byte> indexBytes(semantic::CompactIndexCapacityBytesV1);
    std::memcpy(indexBytes.data(), testCase.artifact.FixedCapacityIndices().data(), indexBytes.size());

    auto indexUpload = CreateBuffer(gpu.device.Get(), D3D12_HEAP_TYPE_UPLOAD, indexBytes.size(),
                                    D3D12_RESOURCE_STATE_GENERIC_READ);
    auto motorUpload = CreateBuffer(gpu.device.Get(), D3D12_HEAP_TYPE_UPLOAD, motorBytes.size(),
                                    D3D12_RESOURCE_STATE_GENERIC_READ);
    auto pointUpload = CreateBuffer(gpu.device.Get(), D3D12_HEAP_TYPE_UPLOAD, pointBytes.size(),
                                    D3D12_RESOURCE_STATE_GENERIC_READ);
    auto zeroUpload = CreateBuffer(gpu.device.Get(), D3D12_HEAP_TYPE_UPLOAD, zeroBytes.size(),
                                   D3D12_RESOURCE_STATE_GENERIC_READ);
    const std::array<std::uint32_t, 3> dispatchArguments{
        testCase.artifact.DispatchGroupsX(), 1u, 1u};
    const auto dispatchArgumentBytes = std::as_bytes(std::span(dispatchArguments));
    auto argumentUpload = CreateBuffer(gpu.device.Get(), D3D12_HEAP_TYPE_UPLOAD, dispatchArgumentBytes.size(),
                                       D3D12_RESOURCE_STATE_GENERIC_READ);
    auto output = CreateBuffer(gpu.device.Get(), D3D12_HEAP_TYPE_DEFAULT, zeroBytes.size(),
                               D3D12_RESOURCE_STATE_COPY_DEST,
                               D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    auto readback = CreateBuffer(gpu.device.Get(), D3D12_HEAP_TYPE_READBACK, zeroBytes.size(),
                                 D3D12_RESOURCE_STATE_COPY_DEST);
    UploadBytes(indexUpload.Get(), indexBytes);
    UploadBytes(motorUpload.Get(), motorBytes);
    UploadBytes(pointUpload.Get(), pointBytes);
    UploadBytes(zeroUpload.Get(), zeroBytes);
    UploadBytes(argumentUpload.Get(), dispatchArgumentBytes);

    gpu.Begin();
    gpu.commandList->CopyBufferRegion(output.Get(), 0, zeroUpload.Get(), 0, zeroBytes.size());
    auto toUav = Transition(output.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    gpu.commandList->ResourceBarrier(1, &toUav);
    gpu.commandList->SetPipelineState(gpu.pipeline.Get());
    gpu.commandList->SetComputeRootSignature(gpu.rootSignature.Get());
    gpu.commandList->SetComputeRoot32BitConstant(0, testCase.artifact.ActiveCount(), 0);
    gpu.commandList->SetComputeRootShaderResourceView(1, indexUpload->GetGPUVirtualAddress());
    gpu.commandList->SetComputeRootShaderResourceView(2, motorUpload->GetGPUVirtualAddress());
    gpu.commandList->SetComputeRootShaderResourceView(3, pointUpload->GetGPUVirtualAddress());
    gpu.commandList->SetComputeRootUnorderedAccessView(4, output->GetGPUVirtualAddress());
    gpu.commandList->ExecuteIndirect(
        gpu.dispatchSignature.Get(), 1, argumentUpload.Get(), 0, nullptr, 0);
    auto uav = UavBarrier(output.Get());
    gpu.commandList->ResourceBarrier(1, &uav);
    auto toCopy = Transition(output.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
    gpu.commandList->ResourceBarrier(1, &toCopy);
    gpu.commandList->CopyBufferRegion(readback.Get(), 0, output.Get(), 0, zeroBytes.size());
    (void)gpu.SubmitAndWait();

    const auto observedBytes = ReadbackBytes(readback.Get(), zeroBytes.size());
    std::vector<spiral5::semantic::PointRecordV1> observed(semantic::WorkUniverseCountV1);
    std::memcpy(observed.data(), observedBytes.data(), observedBytes.size());

    CompactIndexCaseObservationV1 result;
    result.pattern = testCase.pattern;
    result.activeCount = testCase.artifact.ActiveCount();
    result.expectedDispatchGroupsX = result.activeCount == 0
        ? 0u : 1u + ((result.activeCount - 1u) / semantic::ThreadsPerGroupV1);
    result.observedDispatchGroupsX = testCase.artifact.DispatchGroupsX();
    result.sparseSetIdentity = testCase.set.Identity();
    result.artifactIdentity = testCase.artifact.ArtifactIdentity();
    result.outputDigest = base::Sha256(observedBytes);
    result.activeReferenceQualified = true;
    result.inactiveSentinelByteStable = true;
    result.activeHomogeneousWQualified = true;
    result.finiteQualified = true;

    for (std::uint32_t index = 0; index < semantic::WorkUniverseCountV1; ++index)
    {
        const bool active = testCase.set.Contains(index);
        if (!active)
        {
            const auto offset = static_cast<std::size_t>(index) * sizeof(spiral5::semantic::PointRecordV1);
            if (std::memcmp(observedBytes.data() + offset, zeroBytes.data() + offset,
                            sizeof(spiral5::semantic::PointRecordV1)) != 0)
                result.inactiveSentinelByteStable = false;
            continue;
        }

        ++result.activeWriteCount;
        const auto& actual = observed[index];
        const auto& reference = expected[index];
        const std::array<float, 4> a{actual.x, actual.y, actual.z, actual.w};
        const std::array<float, 4> e{reference.x, reference.y, reference.z, reference.w};
        for (std::size_t component = 0; component < a.size(); ++component)
        {
            if (!std::isfinite(a[component])) result.finiteQualified = false;
            const double error = std::abs(static_cast<double>(a[component]) - e[component]);
            result.maxAbsoluteError = std::max(result.maxAbsoluteError, error);
            if (error > 1.0e-6) result.activeReferenceQualified = false;
        }
        if (std::bit_cast<std::uint32_t>(actual.w) != std::bit_cast<std::uint32_t>(1.0f))
            result.activeHomogeneousWQualified = false;
    }

    if (result.observedDispatchGroupsX != result.expectedDispatchGroupsX ||
        result.activeWriteCount != result.activeCount ||
        !result.activeReferenceQualified || !result.inactiveSentinelByteStable ||
        !result.activeHomogeneousWQualified || !result.finiteQualified ||
        observedBytes.size() != expectedBytes.size())
    {
        throw std::runtime_error("Compact index-list WARP observation failed its exact write-set contract.");
    }
    return result;
}
#endif
}

base::Result<CompactIndexArchitectureReportV1, SparseExecutionErrorV1>
ExecuteCompactIndexListArchitectureV1(
    const semantic::SparsePointTransformSemanticV1& semanticValue,
    const std::vector<CompactIndexArchitectureCaseV1>& cases)
{
#if !defined(_WIN32)
    (void)semanticValue;
    (void)cases;
    return base::Result<CompactIndexArchitectureReportV1, SparseExecutionErrorV1>::Failure(
        Error("execution/platform", "Spiral 6 Compact Index architecture requires Windows D3D12."));
#else
    try
    {
        if (cases.empty())
            return base::Result<CompactIndexArchitectureReportV1, SparseExecutionErrorV1>::Failure(
                Error("execution/corpus", "Compact Index architecture corpus is empty."));
        GpuContext gpu;
        CompactIndexArchitectureReportV1 report;
        report.adapterDescription = gpu.adapterDescription;
        report.semanticIdentity = semantic::SparsePointTransformSemanticIdentityV1(semanticValue);
        report.cases.reserve(cases.size());
        for (const auto& testCase : cases)
            report.cases.push_back(ExecuteCase(gpu, semanticValue, testCase));
        return base::Result<CompactIndexArchitectureReportV1, SparseExecutionErrorV1>::Success(
            std::move(report));
    }
    catch (const std::exception& error)
    {
        return base::Result<CompactIndexArchitectureReportV1, SparseExecutionErrorV1>::Failure(
            Error("execution/d3d12", error.what()));
    }
#endif
}

std::vector<std::byte> SerializeCompactIndexArchitectureReportV1(
    const CompactIndexArchitectureReportV1& report)
{
    base::BinaryWriter writer;
    constexpr std::string_view domain = "SGE4-5.Spiral6.CU2.CompactIndexArchitectureEvidence.V1";
    writer.WriteBytes(base::Sha256(std::as_bytes(std::span(domain.data(), domain.size()))));
    writer.WriteBytes(report.semanticIdentity);
    writer.WriteU32(static_cast<std::uint32_t>(report.cases.size()));
    for (const auto& value : report.cases)
    {
        writer.WriteU32(static_cast<std::uint32_t>(value.pattern));
        writer.WriteU32(value.activeCount);
        writer.WriteU32(value.expectedDispatchGroupsX);
        writer.WriteU32(value.observedDispatchGroupsX);
        writer.WriteU32(value.activeWriteCount);
        writer.WriteU32(value.activeReferenceQualified ? 1u : 0u);
        writer.WriteU32(value.inactiveSentinelByteStable ? 1u : 0u);
        writer.WriteU32(value.activeHomogeneousWQualified ? 1u : 0u);
        writer.WriteU32(value.finiteQualified ? 1u : 0u);
        writer.WriteU64(std::bit_cast<std::uint64_t>(value.maxAbsoluteError));
        writer.WriteBytes(value.sparseSetIdentity);
        writer.WriteBytes(value.artifactIdentity);
        writer.WriteBytes(value.outputDigest);
    }
    const auto digest = base::Sha256(writer.Bytes());
    writer.WriteBytes(digest);
    return std::move(writer).Take();
}

namespace
{
base::Digest256 BuildSparseBindingIdentity(
    const verification::VerifiedSparseLoweringV1& verified,
    const SparseIndexResourceBindingInputV1& binding)
{
    constexpr std::string_view domain =
        "SGE4-5.Spiral6.FrozenVerifiedCompactIndexBinding.V1";
    base::BinaryWriter writer;
    writer.WriteBytes(base::Sha256(
        std::as_bytes(std::span(domain.data(), domain.size()))));
    writer.WriteBytes(verified.Plan().planIdentity);
    writer.WriteBytes(verified.CertificateIdentity());
    writer.WriteU64(binding.deviceEpoch);
    writer.WriteBytes(binding.actualIndexResourceIdentity);
    writer.WriteU32(static_cast<std::uint32_t>(binding.representationRole));
    writer.WriteU32(binding.fixedCapacityBytes);
    writer.WriteU32(binding.indexStrideBytes);
    return base::Sha256(writer.Bytes());
}
}

FrozenVerifiedCompactIndexExecutionV1::FrozenVerifiedCompactIndexExecutionV1(
    verification::VerifiedSparseLoweringV1 verified,
    contract::FrozenCompactIndexListArtifactV1 artifact,
    SparseIndexResourceBindingInputV1 binding,
    base::Digest256 bindingIdentity)
    : verified_(std::move(verified)),
      artifact_(std::move(artifact)),
      binding_(binding),
      bindingIdentity_(bindingIdentity)
{
}

base::Result<FrozenVerifiedCompactIndexExecutionV1, std::string>
FreezeVerifiedCompactIndexExecutionV1(
    const verification::VerifiedSparseLoweringV1& verified,
    const semantic::SparsePointTransformSemanticV1& semanticValue,
    const semantic::ExactSparseWorkSetV1& set,
    const contract::FrozenCompactIndexListArtifactV1& artifact,
    const SparseIndexResourceBindingInputV1& binding)
{
    const auto context = verification::BuildCanonicalSparseVerificationContextV1();
    auto verifiedContext = verification::ValidateVerifiedSparseLoweringContextV1(
        verified, semanticValue, set, context);
    if (!verifiedContext)
        return base::Result<FrozenVerifiedCompactIndexExecutionV1, std::string>::Failure(
            verifiedContext.Error());

    auto artifactValidation = contract::ValidateCompactIndexListArtifactForExecutionV1(
        artifact, semanticValue, set);
    if (!artifactValidation)
        return base::Result<FrozenVerifiedCompactIndexExecutionV1, std::string>::Failure(
            artifactValidation.Error().stage + ": " + artifactValidation.Error().message);

    const auto& operation = verified.Plan().operation;
    if (artifact.ArtifactIdentity() != operation.artifactIdentity ||
        artifact.SparseSetIdentity() != verified.Plan().sparseSetIdentity ||
        artifact.RepresentationRole() != operation.representationRole ||
        artifact.ActiveCount() != operation.activeCount ||
        artifact.DispatchGroupsX() != operation.dispatchGroupsX)
    {
        return base::Result<FrozenVerifiedCompactIndexExecutionV1, std::string>::Failure(
            "Frozen Sparse artifact does not match the Verified Plan.");
    }
    if (binding.deviceEpoch == 0)
        return base::Result<FrozenVerifiedCompactIndexExecutionV1, std::string>::Failure(
            "Sparse resource binding requires a non-zero device epoch.");
    if (binding.actualIndexResourceIdentity != operation.indexResourceIdentity)
        return base::Result<FrozenVerifiedCompactIndexExecutionV1, std::string>::Failure(
            "actual Compact Index Resource identity mismatch");
    if (binding.representationRole != operation.representationRole)
        return base::Result<FrozenVerifiedCompactIndexExecutionV1, std::string>::Failure(
            "Compact Index Resource role mismatch");
    if (binding.fixedCapacityBytes != operation.fixedCapacityBytes ||
        binding.indexStrideBytes != operation.indexStrideBytes)
    {
        return base::Result<FrozenVerifiedCompactIndexExecutionV1, std::string>::Failure(
            "Compact Index Resource layout mismatch");
    }

    const auto identity = BuildSparseBindingIdentity(verified, binding);
    return base::Result<FrozenVerifiedCompactIndexExecutionV1, std::string>::Success(
        FrozenVerifiedCompactIndexExecutionV1(
            verified, artifact, binding, identity));
}

base::Result<void, std::string>
ValidateFrozenCompactIndexExecutionEpochV1(
    const FrozenVerifiedCompactIndexExecutionV1& frozen,
    std::uint64_t currentDeviceEpoch)
{
    if (currentDeviceEpoch == 0 ||
        currentDeviceEpoch != frozen.ResourceBinding().deviceEpoch)
    {
        return base::Result<void, std::string>::Failure(
            "frozen Sparse execution belongs to a stale or different device epoch");
    }
    if (frozen.BindingIdentity() !=
        BuildSparseBindingIdentity(frozen.Verified(), frozen.ResourceBinding()))
    {
        return base::Result<void, std::string>::Failure(
            "frozen Sparse resource binding identity mismatch");
    }
    return base::Result<void, std::string>::Success();
}

base::Result<VerifiedCompactIndexExecutionReportV1, SparseExecutionErrorV1>
ExecuteVerifiedCompactIndexListV1(
    const semantic::SparsePointTransformSemanticV1& semanticValue,
    const semantic::ExactSparseWorkSetV1& set,
    const FrozenVerifiedCompactIndexExecutionV1& frozen,
    std::uint64_t currentDeviceEpoch)
{
    auto epoch = ValidateFrozenCompactIndexExecutionEpochV1(
        frozen, currentDeviceEpoch);
    if (!epoch)
        return base::Result<VerifiedCompactIndexExecutionReportV1, SparseExecutionErrorV1>::Failure(
            Error("execution/epoch", epoch.Error()));

    const auto context = verification::BuildCanonicalSparseVerificationContextV1();
    auto verifiedContext = verification::ValidateVerifiedSparseLoweringContextV1(
        frozen.Verified(), semanticValue, set, context);
    if (!verifiedContext)
        return base::Result<VerifiedCompactIndexExecutionReportV1, SparseExecutionErrorV1>::Failure(
            Error("execution/authority", verifiedContext.Error()));

    std::vector<CompactIndexArchitectureCaseV1> cases;
    cases.emplace_back(
        semantic::SparsePatternKindV1::HashScatterPermutation,
        set,
        frozen.Artifact());
    auto architecture = ExecuteCompactIndexListArchitectureV1(
        semanticValue, cases);
    if (!architecture)
        return base::Result<VerifiedCompactIndexExecutionReportV1, SparseExecutionErrorV1>::Failure(
            architecture.Error());

    VerifiedCompactIndexExecutionReportV1 report;
    report.deviceEpoch = currentDeviceEpoch;
    report.bindingIdentity = frozen.BindingIdentity();
    report.architecture = std::move(architecture).Value();
    return base::Result<VerifiedCompactIndexExecutionReportV1, SparseExecutionErrorV1>::Success(
        std::move(report));
}

std::vector<std::byte> SerializeFrozenVerifiedCompactIndexExecutionV1(
    const FrozenVerifiedCompactIndexExecutionV1& value)
{
    constexpr std::string_view domain =
        "SGE4-5.Spiral6.FrozenVerifiedCompactIndexExecution.V1";
    base::BinaryWriter writer;
    writer.WriteBytes(base::Sha256(
        std::as_bytes(std::span(domain.data(), domain.size()))));
    const auto verified = verification::SerializeVerifiedSparseLoweringV1(
        value.Verified());
    writer.WriteU32(static_cast<std::uint32_t>(verified.size()));
    writer.WriteBytes(verified);
    writer.WriteBytes(value.Artifact().Bytes());
    writer.WriteU64(value.ResourceBinding().deviceEpoch);
    writer.WriteBytes(value.ResourceBinding().actualIndexResourceIdentity);
    writer.WriteU32(static_cast<std::uint32_t>(
        value.ResourceBinding().representationRole));
    writer.WriteU32(value.ResourceBinding().fixedCapacityBytes);
    writer.WriteU32(value.ResourceBinding().indexStrideBytes);
    writer.WriteBytes(value.BindingIdentity());
    return std::move(writer).Take();
}

std::vector<std::byte> SerializeVerifiedCompactIndexExecutionReportV1(
    const VerifiedCompactIndexExecutionReportV1& value)
{
    constexpr std::string_view domain =
        "SGE4-5.Spiral6.CU3.VerifiedCompactIndexExecutionEvidence.V1";
    base::BinaryWriter writer;
    writer.WriteBytes(base::Sha256(
        std::as_bytes(std::span(domain.data(), domain.size()))));
    writer.WriteU64(value.deviceEpoch);
    writer.WriteBytes(value.bindingIdentity);
    const auto architecture = SerializeCompactIndexArchitectureReportV1(
        value.architecture);
    writer.WriteU32(static_cast<std::uint32_t>(architecture.size()));
    writer.WriteBytes(architecture);
    const auto digest = base::Sha256(writer.Bytes());
    writer.WriteBytes(digest);
    return std::move(writer).Take();
}

}
