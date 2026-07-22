#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "CandidateFamilyExecution.h"

#include "../00_Foundation/BinaryIO.h"
#include "../121_Spiral4Contracts/Spiral4Contracts.h"
#include "../122_Spiral4IndirectExecution/Spiral4IndirectExecution.h"

#include <Windows.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib")

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace sge4_5::spiral4::family_execution
{
namespace
{
using Microsoft::WRL::ComPtr;

struct Float4 final { float x, y, z, w; };
struct Motor final { Float4 qr, qd; };
struct BatchCommand final
{
    std::uint32_t firstWork;
    std::uint32_t dispatchX;
    std::uint32_t dispatchY;
    std::uint32_t dispatchZ;
};

static_assert(sizeof(Float4) == 16);
static_assert(sizeof(Motor) == 32);
static_assert(sizeof(BatchCommand) == 16);

constexpr Float4 InactiveSentinel{
    123456.25f, -654321.5f, 777.0f, -999.0f
};

base::Digest256 LiteralIdentity(std::string_view value)
{
    return base::Sha256(std::as_bytes(std::span(value.data(), value.size())));
}

CandidateFamilyExecutionErrorV1 Error(
    std::string stage,
    std::string message,
    HRESULT hr = S_OK)
{
    return CandidateFamilyExecutionErrorV1{
        std::move(stage), std::move(message), static_cast<long>(hr)
    };
}

void ThrowIfFailed(HRESULT hr, const char* stage)
{
    if (FAILED(hr))
    {
        std::ostringstream stream;
        stream << stage << " failed with HRESULT 0x"
               << std::hex << static_cast<unsigned long>(hr);
        throw std::runtime_error(stream.str());
    }
}

std::string Utf8FromWide(const wchar_t* value)
{
    if (!value || *value == L'\0') return {};
    const int required = WideCharToMultiByte(
        CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
    if (required <= 1) return {};
    std::string result(static_cast<std::size_t>(required), '\0');
    if (WideCharToMultiByte(
            CP_UTF8, 0, value, -1, result.data(), required,
            nullptr, nullptr) != required)
    {
        throw std::runtime_error("WideCharToMultiByte failed.");
    }
    result.resize(static_cast<std::size_t>(required - 1));
    return result;
}

D3D12_HEAP_PROPERTIES HeapProperties(D3D12_HEAP_TYPE type)
{
    D3D12_HEAP_PROPERTIES value{};
    value.Type = type;
    value.CreationNodeMask = 1;
    value.VisibleNodeMask = 1;
    return value;
}

D3D12_RESOURCE_DESC BufferDescription(
    std::uint64_t size,
    D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE)
{
    D3D12_RESOURCE_DESC value{};
    value.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    value.Width = size;
    value.Height = 1;
    value.DepthOrArraySize = 1;
    value.MipLevels = 1;
    value.Format = DXGI_FORMAT_UNKNOWN;
    value.SampleDesc.Count = 1;
    value.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    value.Flags = flags;
    return value;
}

ComPtr<ID3D12Resource> CreateBuffer(
    ID3D12Device* device,
    D3D12_HEAP_TYPE heapType,
    std::uint64_t size,
    D3D12_RESOURCE_STATES initialState,
    D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE)
{
    const auto heap = HeapProperties(heapType);
    const auto desc = BufferDescription(size, flags);
    ComPtr<ID3D12Resource> resource;
    ThrowIfFailed(
        device->CreateCommittedResource(
            &heap, D3D12_HEAP_FLAG_NONE, &desc, initialState,
            nullptr, IID_PPV_ARGS(&resource)),
        "CreateCommittedResource");
    return resource;
}

D3D12_RESOURCE_BARRIER TransitionBarrier(
    ID3D12Resource* resource,
    D3D12_RESOURCE_STATES before,
    D3D12_RESOURCE_STATES after)
{
    D3D12_RESOURCE_BARRIER value{};
    value.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    value.Transition.pResource = resource;
    value.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    value.Transition.StateBefore = before;
    value.Transition.StateAfter = after;
    return value;
}

D3D12_RESOURCE_BARRIER UavBarrier(ID3D12Resource* resource)
{
    D3D12_RESOURCE_BARRIER value{};
    value.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    value.UAV.pResource = resource;
    return value;
}

ComPtr<ID3DBlob> CompileShader(
    const std::string& source,
    const char* stage)
{
    ComPtr<ID3DBlob> bytecode;
    ComPtr<ID3DBlob> errors;
    const UINT flags =
#if defined(_DEBUG)
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION |
#endif
        D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS;
    const HRESULT hr = D3DCompile(
        source.data(), source.size(), "Spiral4CU4Generated.hlsl",
        nullptr, nullptr, "CSMain", "cs_5_1", flags, 0,
        &bytecode, &errors);
    if (FAILED(hr))
    {
        std::string message = stage;
        message += " HLSL compilation failed";
        if (errors && errors->GetBufferPointer())
        {
            message += ": ";
            message.append(
                static_cast<const char*>(errors->GetBufferPointer()),
                errors->GetBufferSize());
        }
        throw std::runtime_error(message);
    }
    return bytecode;
}

ComPtr<ID3D12RootSignature> CreateRootSignature(
    ID3D12Device* device,
    std::span<const D3D12_ROOT_PARAMETER> parameters)
{
    D3D12_ROOT_SIGNATURE_DESC desc{};
    desc.NumParameters = static_cast<UINT>(parameters.size());
    desc.pParameters = parameters.data();
    ComPtr<ID3DBlob> bytes;
    ComPtr<ID3DBlob> errors;
    ThrowIfFailed(
        D3D12SerializeRootSignature(
            &desc, D3D_ROOT_SIGNATURE_VERSION_1, &bytes, &errors),
        "D3D12SerializeRootSignature");
    ComPtr<ID3D12RootSignature> result;
    ThrowIfFailed(
        device->CreateRootSignature(
            0, bytes->GetBufferPointer(), bytes->GetBufferSize(),
            IID_PPV_ARGS(&result)),
        "CreateRootSignature");
    return result;
}

ComPtr<ID3D12PipelineState> CreateComputePipeline(
    ID3D12Device* device,
    ID3D12RootSignature* root,
    ID3DBlob* shader)
{
    D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};
    desc.pRootSignature = root;
    desc.CS.pShaderBytecode = shader->GetBufferPointer();
    desc.CS.BytecodeLength = shader->GetBufferSize();
    ComPtr<ID3D12PipelineState> result;
    ThrowIfFailed(
        device->CreateComputePipelineState(
            &desc, IID_PPV_ARGS(&result)),
        "CreateComputePipelineState");
    return result;
}

D3D12_CPU_DESCRIPTOR_HANDLE CpuHandle(
    D3D12_CPU_DESCRIPTOR_HANDLE start,
    UINT increment,
    UINT index)
{
    start.ptr += static_cast<SIZE_T>(increment) * index;
    return start;
}

D3D12_GPU_DESCRIPTOR_HANDLE GpuHandle(
    D3D12_GPU_DESCRIPTOR_HANDLE start,
    UINT increment,
    UINT index)
{
    start.ptr += static_cast<UINT64>(increment) * index;
    return start;
}

std::string BatchProducerSource()
{
    return R"hlsl(
static const uint MaxWorkCount = 4096u;
static const uint ThreadsPerGroup = 64u;
cbuffer Config : register(b0) { uint BatchSize; uint MaxSlots; };
struct BatchCommand { uint firstWork; uint3 dispatch; };
StructuredBuffer<uint> ActiveCount : register(t0);
RWStructuredBuffer<BatchCommand> Commands : register(u0);
[numthreads(64,1,1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    uint slot = id.x;
    if (slot >= MaxSlots) return;
    uint active = min(ActiveCount[0], MaxWorkCount);
    uint first = slot * BatchSize;
    uint remaining = first < active ? active - first : 0;
    uint workCount = min(BatchSize, remaining);
    uint groups = workCount == 0 ? 0 : 1 + ((workCount - 1) / ThreadsPerGroup);
    BatchCommand command;
    command.firstWork = first;
    command.dispatch = uint3(groups, 1, 1);
    Commands[slot] = command;
}
)hlsl";
}

std::string ConsumerSource()
{
    return R"hlsl(
static const uint MaxWorkCount = 4096u;
static const uint ReusePerBone = 512u;
cbuffer BatchBase : register(b0) { uint FirstWork; };
struct Motor { float4 qr; float4 qd; };
float4 QMul(float4 a, float4 b)
{
    return float4(
        a.x*b.x-dot(a.yzw,b.yzw),
        a.x*b.yzw+b.x*a.yzw+cross(a.yzw,b.yzw));
}
float4 QConj(float4 q) { return float4(q.x,-q.y,-q.z,-q.w); }
StructuredBuffer<Motor> Transforms : register(t0);
StructuredBuffer<float4> Points : register(t1);
StructuredBuffer<uint> ActiveCount : register(t2);
RWStructuredBuffer<float4> Output : register(u0);
[numthreads(64,1,1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    uint n = FirstWork + id.x;
    uint active = min(ActiveCount[0], MaxWorkCount);
    if (n >= active) return;
    uint bone = n / ReusePerBone;
    Motor m = Transforms[bone];
    float3 p = Points[n].xyz;
    float3 q = QMul(QMul(m.qr, float4(0,p)), QConj(m.qr)).yzw;
    float3 t = 2.0f * QMul(m.qd, QConj(m.qr)).yzw;
    Output[n] = float4(q+t, 1.0f);
}
)hlsl";
}

std::vector<Motor> BuildTransforms()
{
    std::vector<Motor> values(8);
    for (std::uint32_t bone = 0; bone < values.size(); ++bone)
    {
        const float tx = static_cast<float>(bone) * 0.5f;
        const float ty = -static_cast<float>(bone) * 0.25f;
        const float tz = static_cast<float>(bone) * 0.125f;
        values[bone].qr = Float4{1.0f, 0.0f, 0.0f, 0.0f};
        values[bone].qd = Float4{0.0f, tx * 0.5f, ty * 0.5f, tz * 0.5f};
    }
    return values;
}

std::vector<Float4> BuildPoints(std::uint32_t count)
{
    std::vector<Float4> values(count);
    for (std::uint32_t i = 0; i < count; ++i)
    {
        values[i] = Float4{
            static_cast<float>(i % 17u) * 0.25f,
            -static_cast<float>((i / 17u) % 13u) * 0.125f,
            static_cast<float>(i % 7u) * 0.5f,
            1.0f
        };
    }
    return values;
}

Float4 ExpectedPoint(const Float4& point, std::uint32_t index)
{
    const std::uint32_t bone = index / 512u;
    return Float4{
        point.x + static_cast<float>(bone) * 0.5f,
        point.y - static_cast<float>(bone) * 0.25f,
        point.z + static_cast<float>(bone) * 0.125f,
        1.0f
    };
}

bool IsSentinel(const Float4& value)
{
    return std::memcmp(&value, &InactiveSentinel, sizeof(Float4)) == 0;
}

base::Digest256 BuildBindingIdentity(
    const family_verification::VerifiedCandidateFamilyV1& verified,
    const family_contract::FrozenCandidateFamilyArtifactV1& artifact)
{
    base::BinaryWriter writer;
    writer.WriteBytes(LiteralIdentity(
        "SGE4-5.Spiral4.FrozenVerifiedCandidateFamilyBinding.V1"));
    writer.WriteBytes(verified.Plan().planIdentity);
    writer.WriteBytes(verified.CertificateIdentity());
    writer.WriteBytes(artifact.ArtifactIdentity());
    writer.WriteBytes(artifact.SemanticIdentity());
    writer.WriteBytes(verified.Plan().producerProgramTemplateIdentity);
    writer.WriteBytes(verified.Plan().consumerProgramTemplateIdentity);
    return base::Sha256(std::move(writer).Take());
}

base::Result<void, std::string> ValidateFrozen(
    const FrozenVerifiedCandidateFamilyV1& frozen,
    const semantic::ActiveWorkSemanticV1& semantic,
    const family_verification::CandidateFamilyVerificationContextV1& context)
{
    auto valid =
        family_verification::ValidateVerifiedCandidateFamilyContextV1(
            frozen.Verified(), semantic, context);
    if (!valid) return valid;

    const auto& plan = frozen.Verified().Plan();
    const auto& artifact = frozen.Artifact();
    if (plan.artifactIdentity != artifact.ArtifactIdentity() ||
        plan.kind != artifact.Kind() ||
        plan.commandSignaturePolicy != artifact.CommandSignaturePolicy() ||
        plan.issuancePolicy != artifact.IssuancePolicy() ||
        plan.batchSize != artifact.BatchSize() ||
        plan.maxBatchSlots != artifact.MaxBatchSlots() ||
        plan.maxWorkCount != artifact.MaxWorkCount() ||
        plan.threadsPerGroup != artifact.ThreadsPerGroup() ||
        plan.directDispatchX != artifact.DirectDispatchX() ||
        plan.argumentStrideBytes != artifact.ArgumentStrideBytes() ||
        plan.argumentBufferBytes != artifact.ArgumentBufferBytes() ||
        plan.maxCommandCount != artifact.MaxCommandCount() ||
        plan.producerDispatchX != artifact.ProducerDispatchX())
    {
        return base::Result<void, std::string>::Failure(
            "Verified Candidate family is not bound to the actual artifact.");
    }

    if (BuildBindingIdentity(frozen.Verified(), frozen.Artifact()) !=
        frozen.BindingIdentity())
    {
        return base::Result<void, std::string>::Failure(
            "Candidate family Frozen binding identity mismatch.");
    }
    return base::Result<void, std::string>::Success();
}

struct EventHandle final
{
    HANDLE value = nullptr;
    ~EventHandle() { if (value) CloseHandle(value); }
};
}

FrozenVerifiedCandidateFamilyV1::FrozenVerifiedCandidateFamilyV1(
    family_verification::VerifiedCandidateFamilyV1 verified,
    family_contract::FrozenCandidateFamilyArtifactV1 artifact,
    base::Digest256 bindingIdentity)
    : verified_(std::move(verified)),
      artifact_(std::move(artifact)),
      bindingIdentity_(bindingIdentity)
{
}

base::Result<FrozenVerifiedCandidateFamilyV1, std::string>
FreezeVerifiedCandidateFamilyV1(
    const semantic::ActiveWorkSemanticV1& semantic,
    const family_verification::CandidateFamilyVerificationContextV1& context,
    const family_verification::VerifiedCandidateFamilyV1& verified)
{
    auto valid =
        family_verification::ValidateVerifiedCandidateFamilyContextV1(
            verified, semantic, context);
    if (!valid)
    {
        return base::Result<FrozenVerifiedCandidateFamilyV1, std::string>::Failure(
            valid.Error());
    }

    auto artifact = family_contract::BuildCandidateFamilyArtifactV1(
        semantic, verified.Plan().kind, verified.Plan().batchSize);
    if (!artifact)
    {
        return base::Result<FrozenVerifiedCandidateFamilyV1, std::string>::Failure(
            artifact.Error().stage + ": " + artifact.Error().message);
    }

    const auto binding = BuildBindingIdentity(verified, artifact.Value());
    FrozenVerifiedCandidateFamilyV1 frozen(
        verified, std::move(artifact.Value()), binding);
    auto frozenValid = ValidateFrozen(frozen, semantic, context);
    if (!frozenValid)
    {
        return base::Result<FrozenVerifiedCandidateFamilyV1, std::string>::Failure(
            frozenValid.Error());
    }
    return base::Result<FrozenVerifiedCandidateFamilyV1, std::string>::Success(
        std::move(frozen));
}

std::vector<std::byte>
SerializeFrozenVerifiedCandidateFamilyV1(
    const FrozenVerifiedCandidateFamilyV1& value)
{
    base::BinaryWriter writer;
    writer.WriteBytes(LiteralIdentity(
        "SGE4-5.Spiral4.FrozenVerifiedCandidateFamily.V1"));
    const auto verifiedBytes =
        family_verification::SerializeVerifiedCandidateFamilyV1(
            value.Verified());
    writer.WriteU32(static_cast<std::uint32_t>(verifiedBytes.size()));
    writer.WriteBytes(verifiedBytes);
    writer.WriteU32(static_cast<std::uint32_t>(
        value.Artifact().Bytes().size()));
    writer.WriteBytes(value.Artifact().Bytes());
    writer.WriteBytes(value.BindingIdentity());
    return std::move(writer).Take();
}

base::Result<
    std::vector<CandidateFamilyRunResultV1>,
    CandidateFamilyExecutionErrorV1>
RunVerifiedCandidateFamilyCorpusOnWarpV1(
    const semantic::ActiveWorkSemanticV1& semantic,
    const family_verification::CandidateFamilyVerificationContextV1& context,
    std::span<const FrozenVerifiedCandidateFamilyV1> candidates,
    std::span<const std::uint32_t> activeCounts)
{
    try
    {
        if (candidates.empty() || activeCounts.empty())
        {
            return base::Result<
                std::vector<CandidateFamilyRunResultV1>,
                CandidateFamilyExecutionErrorV1>::Failure(
                    Error("input", "Candidate and Active Count corpora must be nonempty."));
        }

        for (const auto& candidate : candidates)
        {
            auto valid = ValidateFrozen(candidate, semantic, context);
            if (!valid)
            {
                return base::Result<
                    std::vector<CandidateFamilyRunResultV1>,
                    CandidateFamilyExecutionErrorV1>::Failure(
                        Error("authority", valid.Error()));
            }
        }
        for (const auto active : activeCounts)
        {
            if (active > semantic.maxWorkCount.Value())
            {
                return base::Result<
                    std::vector<CandidateFamilyRunResultV1>,
                    CandidateFamilyExecutionErrorV1>::Failure(
                        Error("active-count", "Active Count exceeds Nmax."));
            }
        }

        std::vector<CandidateFamilyRunResultV1> results;
        results.reserve(candidates.size());

        for (const auto& candidate : candidates)
        {
            if (candidate.Verified().Plan().kind ==
                family_contract::CandidateFamilyKindV1::
                    SingleExecuteIndirectDispatch)
            {
                auto cu2Artifact =
                    contract::BuildCu2SingleIndirectArtifactV1(semantic);
                auto singleRun =
                    execution::RunSingleIndirectArchitectureOnWarpV1(
                        semantic, cu2Artifact, activeCounts);
                if (!singleRun)
                {
                    return base::Result<
                        std::vector<CandidateFamilyRunResultV1>,
                        CandidateFamilyExecutionErrorV1>::Failure(
                            Error(singleRun.Error().stage,
                                  singleRun.Error().message,
                                  static_cast<HRESULT>(
                                      singleRun.Error().hresult)));
                }

                CandidateFamilyRunResultV1 converted;
                converted.kind = candidate.Verified().Plan().kind;
                converted.batchSize = 0;
                converted.adapterDescription =
                    singleRun.Value().adapterDescription;
                converted.producerShaderBytecodeDigest =
                    singleRun.Value().producerShaderBytecodeDigest;
                converted.consumerShaderBytecodeDigest =
                    singleRun.Value().consumerShaderBytecodeDigest;
                converted.verifiedBindingIdentity =
                    candidate.BindingIdentity();

                for (const auto& source : singleRun.Value().cases)
                {
                    CandidateFamilyCaseResultV1 target;
                    target.activeWorkCount = source.activeWorkCount;
                    target.issuedThreadGroupCount =
                        source.observedDispatchGroupCountX;
                    target.activeMismatchCount =
                        source.activeMismatchCount;
                    target.firstActiveMismatch =
                        source.firstActiveMismatch;
                    target.inactiveTailPreserved =
                        source.inactiveTailPreserved;
                    target.maxAbsoluteError =
                        source.maxAbsoluteError;
                    target.outputDigest =
                        source.readbackDigest;
                    target.observedBatchRecords.push_back(
                        family_contract::BatchDispatchRecordV1{
                            0,
                            source.observedDispatchGroupCountX,
                            1,
                            1});
                    converted.cases.push_back(std::move(target));
                }

                base::BinaryWriter identity;
                identity.WriteBytes(LiteralIdentity(
                    "SGE4-5.Spiral4.ActualCandidateFamilyExecution.V1"));
                identity.WriteBytes(converted.verifiedBindingIdentity);
                identity.WriteBytes(
                    converted.producerShaderBytecodeDigest);
                identity.WriteBytes(
                    converted.consumerShaderBytecodeDigest);
                identity.WriteU32(
                    static_cast<std::uint32_t>(converted.kind));
                identity.WriteU32(0);
                converted.actualExecutionIdentity =
                    base::Sha256(std::move(identity).Take());
                results.push_back(std::move(converted));
            }
        }

        ComPtr<IDXGIFactory6> factory;
        ThrowIfFailed(
            CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)),
            "CreateDXGIFactory2");
        ComPtr<IDXGIAdapter> warpAdapter;
        ThrowIfFailed(
            factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)),
            "EnumWarpAdapter");
        DXGI_ADAPTER_DESC adapterDesc{};
        ThrowIfFailed(warpAdapter->GetDesc(&adapterDesc), "GetDesc");

        ComPtr<ID3D12Device> device;
        ThrowIfFailed(
            D3D12CreateDevice(
                warpAdapter.Get(), D3D_FEATURE_LEVEL_11_0,
                IID_PPV_ARGS(&device)),
            "D3D12CreateDevice");

        D3D12_COMMAND_QUEUE_DESC queueDesc{};
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
        ComPtr<ID3D12CommandQueue> queue;
        ThrowIfFailed(
            device->CreateCommandQueue(
                &queueDesc, IID_PPV_ARGS(&queue)),
            "CreateCommandQueue");

        ComPtr<ID3D12CommandAllocator> allocator;
        ThrowIfFailed(
            device->CreateCommandAllocator(
                D3D12_COMMAND_LIST_TYPE_COMPUTE,
                IID_PPV_ARGS(&allocator)),
            "CreateCommandAllocator");

        ComPtr<ID3D12GraphicsCommandList> list;
        ThrowIfFailed(
            device->CreateCommandList(
                0, D3D12_COMMAND_LIST_TYPE_COMPUTE,
                allocator.Get(), nullptr, IID_PPV_ARGS(&list)),
            "CreateCommandList");
        ThrowIfFailed(list->Close(), "Initial command-list close");

        D3D12_DESCRIPTOR_RANGE batchRanges[2]{};
        batchRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        batchRanges[0].NumDescriptors = 1;
        batchRanges[0].BaseShaderRegister = 0;
        batchRanges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
        batchRanges[1].NumDescriptors = 1;
        batchRanges[1].BaseShaderRegister = 0;

        D3D12_ROOT_PARAMETER batchParams[3]{};
        batchParams[0].ParameterType =
            D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        batchParams[0].Constants.ShaderRegister = 0;
        batchParams[0].Constants.Num32BitValues = 2;
        for (UINT i = 0; i < 2; ++i)
        {
            batchParams[i + 1].ParameterType =
                D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            batchParams[i + 1].DescriptorTable.NumDescriptorRanges = 1;
            batchParams[i + 1].DescriptorTable.pDescriptorRanges =
                &batchRanges[i];
        }

        D3D12_DESCRIPTOR_RANGE consumerRanges[2]{};
        consumerRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        consumerRanges[0].NumDescriptors = 3;
        consumerRanges[0].BaseShaderRegister = 0;
        consumerRanges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
        consumerRanges[1].NumDescriptors = 1;
        consumerRanges[1].BaseShaderRegister = 0;

        D3D12_ROOT_PARAMETER consumerParams[3]{};
        consumerParams[0].ParameterType =
            D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        consumerParams[0].Constants.ShaderRegister = 0;
        consumerParams[0].Constants.Num32BitValues = 1;
        for (UINT i = 0; i < 2; ++i)
        {
            consumerParams[i + 1].ParameterType =
                D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            consumerParams[i + 1].DescriptorTable.NumDescriptorRanges = 1;
            consumerParams[i + 1].DescriptorTable.pDescriptorRanges =
                &consumerRanges[i];
        }

        auto batchRoot = CreateRootSignature(device.Get(), batchParams);
        auto consumerRoot = CreateRootSignature(device.Get(), consumerParams);
        auto batchShader = CompileShader(
            BatchProducerSource(), "Batch Argument Producer");
        auto consumerShader = CompileShader(
            ConsumerSource(), "Candidate Family Consumer");
        auto batchPipeline = CreateComputePipeline(
            device.Get(), batchRoot.Get(), batchShader.Get());
        auto consumerPipeline = CreateComputePipeline(
            device.Get(), consumerRoot.Get(), consumerShader.Get());

        const auto batchDigest = base::Sha256(
            std::span<const std::byte>(
                static_cast<const std::byte*>(
                    batchShader->GetBufferPointer()),
                batchShader->GetBufferSize()));
        const auto consumerDigest = base::Sha256(
            std::span<const std::byte>(
                static_cast<const std::byte*>(
                    consumerShader->GetBufferPointer()),
                consumerShader->GetBufferSize()));

        D3D12_INDIRECT_ARGUMENT_DESC batchArgs[2]{};
        batchArgs[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT;
        batchArgs[0].Constant.RootParameterIndex = 0;
        batchArgs[0].Constant.DestOffsetIn32BitValues = 0;
        batchArgs[0].Constant.Num32BitValuesToSet = 1;
        batchArgs[1].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;

        D3D12_COMMAND_SIGNATURE_DESC signatureDesc{};
        signatureDesc.ByteStride = sizeof(BatchCommand);
        signatureDesc.NumArgumentDescs = 2;
        signatureDesc.pArgumentDescs = batchArgs;
        ComPtr<ID3D12CommandSignature> batchSignature;
        ThrowIfFailed(
            device->CreateCommandSignature(
                &signatureDesc, consumerRoot.Get(),
                IID_PPV_ARGS(&batchSignature)),
            "Create Batch Command Signature");

        const std::uint64_t transformBytes = 8ull * sizeof(Motor);
        const std::uint64_t workBytes =
            static_cast<std::uint64_t>(
                semantic.maxWorkCount.Value()) * sizeof(Float4);
        const std::uint64_t activeCountBytes = sizeof(std::uint32_t);
        const std::uint64_t batchArgumentBytes =
            64ull * sizeof(BatchCommand);

        auto transforms = CreateBuffer(
            device.Get(), D3D12_HEAP_TYPE_UPLOAD,
            transformBytes, D3D12_RESOURCE_STATE_GENERIC_READ);
        auto points = CreateBuffer(
            device.Get(), D3D12_HEAP_TYPE_UPLOAD,
            workBytes, D3D12_RESOURCE_STATE_GENERIC_READ);
        auto activeCount = CreateBuffer(
            device.Get(), D3D12_HEAP_TYPE_UPLOAD,
            activeCountBytes, D3D12_RESOURCE_STATE_GENERIC_READ);
        auto sentinelUpload = CreateBuffer(
            device.Get(), D3D12_HEAP_TYPE_UPLOAD,
            workBytes, D3D12_RESOURCE_STATE_GENERIC_READ);
        auto batchArguments = CreateBuffer(
            device.Get(), D3D12_HEAP_TYPE_DEFAULT,
            batchArgumentBytes,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        auto output = CreateBuffer(
            device.Get(), D3D12_HEAP_TYPE_DEFAULT,
            workBytes, D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        auto outputReadback = CreateBuffer(
            device.Get(), D3D12_HEAP_TYPE_READBACK,
            workBytes, D3D12_RESOURCE_STATE_COPY_DEST);
        auto batchReadback = CreateBuffer(
            device.Get(), D3D12_HEAP_TYPE_READBACK,
            batchArgumentBytes, D3D12_RESOURCE_STATE_COPY_DEST);

        const auto transformValues = BuildTransforms();
        const auto pointValues = BuildPoints(semantic.maxWorkCount.Value());
        std::vector<Float4> sentinels(
            semantic.maxWorkCount.Value(), InactiveSentinel);

        void* mapped = nullptr;
        ThrowIfFailed(
            transforms->Map(0, nullptr, &mapped),
            "Map transforms");
        std::memcpy(mapped, transformValues.data(), transformBytes);
        transforms->Unmap(0, nullptr);

        ThrowIfFailed(points->Map(0, nullptr, &mapped), "Map points");
        std::memcpy(mapped, pointValues.data(), workBytes);
        points->Unmap(0, nullptr);

        ThrowIfFailed(
            sentinelUpload->Map(0, nullptr, &mapped),
            "Map sentinel");
        std::memcpy(mapped, sentinels.data(), workBytes);
        sentinelUpload->Unmap(0, nullptr);

        std::uint32_t* mappedActive = nullptr;
        ThrowIfFailed(
            activeCount->Map(
                0, nullptr,
                reinterpret_cast<void**>(&mappedActive)),
            "Map Active Count");

        D3D12_RANGE outputReadRange{
            0,
            static_cast<SIZE_T>(workBytes)
        };
        D3D12_RANGE batchReadRange{
            0,
            static_cast<SIZE_T>(batchArgumentBytes)
        };
        Float4* mappedOutput = nullptr;
        BatchCommand* mappedBatch = nullptr;
        ThrowIfFailed(
            outputReadback->Map(
                0, &outputReadRange,
                reinterpret_cast<void**>(&mappedOutput)),
            "Map output readback");
        ThrowIfFailed(
            batchReadback->Map(
                0, &batchReadRange,
                reinterpret_cast<void**>(&mappedBatch)),
            "Map Batch readback");

        D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heapDesc.NumDescriptors = 6;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        ComPtr<ID3D12DescriptorHeap> descriptorHeap;
        ThrowIfFailed(
            device->CreateDescriptorHeap(
                &heapDesc, IID_PPV_ARGS(&descriptorHeap)),
            "CreateDescriptorHeap");

        const UINT increment =
            device->GetDescriptorHandleIncrementSize(
                D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        const auto cpuStart =
            descriptorHeap->GetCPUDescriptorHandleForHeapStart();
        const auto gpuStart =
            descriptorHeap->GetGPUDescriptorHandleForHeapStart();

        auto createSrv = [&](UINT index, ID3D12Resource* resource,
                             UINT count, UINT stride)
        {
            D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
            desc.Shader4ComponentMapping =
                D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            desc.Buffer.NumElements = count;
            desc.Buffer.StructureByteStride = stride;
            device->CreateShaderResourceView(
                resource, &desc,
                CpuHandle(cpuStart, increment, index));
        };

        auto createUav = [&](UINT index, ID3D12Resource* resource,
                             UINT count, UINT stride)
        {
            D3D12_UNORDERED_ACCESS_VIEW_DESC desc{};
            desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
            desc.Buffer.NumElements = count;
            desc.Buffer.StructureByteStride = stride;
            device->CreateUnorderedAccessView(
                resource, nullptr, &desc,
                CpuHandle(cpuStart, increment, index));
        };

        createSrv(0, activeCount.Get(), 1, sizeof(std::uint32_t));
        createUav(1, batchArguments.Get(), 64, sizeof(BatchCommand));
        createSrv(2, transforms.Get(), 8, sizeof(Motor));
        createSrv(
            3, points.Get(),
            semantic.maxWorkCount.Value(), sizeof(Float4));
        createSrv(4, activeCount.Get(), 1, sizeof(std::uint32_t));
        createUav(
            5, output.Get(),
            semantic.maxWorkCount.Value(), sizeof(Float4));

        ComPtr<ID3D12Fence> fence;
        ThrowIfFailed(
            device->CreateFence(
                0, D3D12_FENCE_FLAG_NONE,
                IID_PPV_ARGS(&fence)),
            "CreateFence");
        EventHandle event;
        event.value = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!event.value)
        {
            throw std::runtime_error("CreateEventW failed.");
        }
        UINT64 fenceValue = 0;

        for (const auto& candidate : candidates)
        {
            const auto kind = candidate.Verified().Plan().kind;
            if (kind ==
                family_contract::CandidateFamilyKindV1::
                    SingleExecuteIndirectDispatch)
            {
                continue;
            }

            CandidateFamilyRunResultV1 run;
            run.kind = kind;
            run.batchSize = candidate.Verified().Plan().batchSize;
            run.adapterDescription =
                Utf8FromWide(adapterDesc.Description);
            run.producerShaderBytecodeDigest =
                kind ==
                    family_contract::CandidateFamilyKindV1::
                        BatchedExecuteIndirectDispatch
                    ? batchDigest
                    : LiteralIdentity(
                        "SGE4-5.Spiral4.NoArgumentProducerBytecode.V1");
            run.consumerShaderBytecodeDigest = consumerDigest;
            run.verifiedBindingIdentity =
                candidate.BindingIdentity();

            for (const auto active : activeCounts)
            {
                *mappedActive = active;
                ThrowIfFailed(
                    allocator->Reset(), "Allocator Reset");
                ThrowIfFailed(
                    list->Reset(allocator.Get(), nullptr),
                    "List Reset");

                list->CopyBufferRegion(
                    output.Get(), 0,
                    sentinelUpload.Get(), 0,
                    workBytes);
                auto outputToUav = TransitionBarrier(
                    output.Get(),
                    D3D12_RESOURCE_STATE_COPY_DEST,
                    D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
                list->ResourceBarrier(1, &outputToUav);

                ID3D12DescriptorHeap* heaps[] = {
                    descriptorHeap.Get()
                };
                list->SetDescriptorHeaps(1, heaps);

                if (kind ==
                    family_contract::CandidateFamilyKindV1::
                        BatchedExecuteIndirectDispatch)
                {
                    list->SetPipelineState(batchPipeline.Get());
                    list->SetComputeRootSignature(batchRoot.Get());
                    list->SetComputeRoot32BitConstant(
                        0,
                        candidate.Verified().Plan().batchSize,
                        0);
                    list->SetComputeRoot32BitConstant(
                        0,
                        candidate.Verified().Plan().maxBatchSlots,
                        1);
                    list->SetComputeRootDescriptorTable(
                        1,
                        GpuHandle(gpuStart, increment, 0));
                    list->SetComputeRootDescriptorTable(
                        2,
                        GpuHandle(gpuStart, increment, 1));
                    list->Dispatch(
                        candidate.Verified().Plan().producerDispatchX,
                        1, 1);

                    auto argumentUav =
                        UavBarrier(batchArguments.Get());
                    list->ResourceBarrier(1, &argumentUav);
                    auto toIndirect = TransitionBarrier(
                        batchArguments.Get(),
                        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                        D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
                    list->ResourceBarrier(1, &toIndirect);
                }

                list->SetPipelineState(consumerPipeline.Get());
                list->SetComputeRootSignature(consumerRoot.Get());
                list->SetComputeRoot32BitConstant(0, 0, 0);
                list->SetComputeRootDescriptorTable(
                    1,
                    GpuHandle(gpuStart, increment, 2));
                list->SetComputeRootDescriptorTable(
                    2,
                    GpuHandle(gpuStart, increment, 5));

                if (kind ==
                    family_contract::CandidateFamilyKindV1::
                        FixedMaximumGuarded)
                {
                    list->Dispatch(
                        candidate.Verified().Plan().directDispatchX,
                        1, 1);
                }
                else
                {
                    list->ExecuteIndirect(
                        batchSignature.Get(),
                        candidate.Verified().Plan().maxCommandCount,
                        batchArguments.Get(),
                        0,
                        nullptr,
                        0);
                }

                auto outputUav = UavBarrier(output.Get());
                list->ResourceBarrier(1, &outputUav);
                auto outputToCopy = TransitionBarrier(
                    output.Get(),
                    D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                    D3D12_RESOURCE_STATE_COPY_SOURCE);
                list->ResourceBarrier(1, &outputToCopy);

                if (kind ==
                    family_contract::CandidateFamilyKindV1::
                        BatchedExecuteIndirectDispatch)
                {
                    auto argsToCopy = TransitionBarrier(
                        batchArguments.Get(),
                        D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT,
                        D3D12_RESOURCE_STATE_COPY_SOURCE);
                    list->ResourceBarrier(1, &argsToCopy);
                    list->CopyBufferRegion(
                        batchReadback.Get(), 0,
                        batchArguments.Get(), 0,
                        candidate.Verified().Plan().
                            argumentBufferBytes);
                    auto argsBack = TransitionBarrier(
                        batchArguments.Get(),
                        D3D12_RESOURCE_STATE_COPY_SOURCE,
                        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
                    list->ResourceBarrier(1, &argsBack);
                }

                list->CopyBufferRegion(
                    outputReadback.Get(), 0,
                    output.Get(), 0,
                    workBytes);
                auto outputBack = TransitionBarrier(
                    output.Get(),
                    D3D12_RESOURCE_STATE_COPY_SOURCE,
                    D3D12_RESOURCE_STATE_COPY_DEST);
                list->ResourceBarrier(1, &outputBack);

                ThrowIfFailed(list->Close(), "List Close");
                ID3D12CommandList* commandLists[] = {
                    list.Get()
                };
                queue->ExecuteCommandLists(1, commandLists);
                ++fenceValue;
                ThrowIfFailed(
                    queue->Signal(fence.Get(), fenceValue),
                    "Queue Signal");
                if (fence->GetCompletedValue() < fenceValue)
                {
                    ThrowIfFailed(
                        fence->SetEventOnCompletion(
                            fenceValue, event.value),
                        "SetEventOnCompletion");
                    WaitForSingleObject(event.value, INFINITE);
                }

                CandidateFamilyCaseResultV1 caseResult;
                caseResult.activeWorkCount = active;
                caseResult.inactiveTailPreserved = true;

                if (kind ==
                    family_contract::CandidateFamilyKindV1::
                        FixedMaximumGuarded)
                {
                    caseResult.issuedThreadGroupCount =
                        candidate.Verified().Plan().directDispatchX;
                }
                else
                {
                    for (std::uint32_t slot = 0;
                         slot <
                            candidate.Verified().Plan().
                                maxBatchSlots;
                         ++slot)
                    {
                        const auto& record = mappedBatch[slot];
                        caseResult.issuedThreadGroupCount +=
                            record.dispatchX;
                        caseResult.observedBatchRecords.push_back(
                            family_contract::BatchDispatchRecordV1{
                                record.firstWork,
                                record.dispatchX,
                                record.dispatchY,
                                record.dispatchZ});
                    }

                    auto expected =
                        family_verification::
                            DeriveVerifiedBatchDispatchRecordsV1(
                                candidate.Verified(),
                                semantic::ActiveWorkCountV1(
                                    active,
                                    semantic.maxWorkCount));
                    if (!expected ||
                        expected.Value() !=
                            caseResult.observedBatchRecords)
                    {
                        return base::Result<
                            std::vector<CandidateFamilyRunResultV1>,
                            CandidateFamilyExecutionErrorV1>::Failure(
                                Error(
                                    "batch-observation",
                                    "GPU Batch records differ from the Verified partition."));
                    }
                }

                for (std::uint32_t i = 0; i < active; ++i)
                {
                    const Float4 expected =
                        ExpectedPoint(pointValues[i], i);
                    const Float4 actual = mappedOutput[i];
                    const float errors[] = {
                        std::abs(actual.x - expected.x),
                        std::abs(actual.y - expected.y),
                        std::abs(actual.z - expected.z),
                        std::abs(actual.w - expected.w)
                    };
                    const float local =
                        *std::max_element(
                            std::begin(errors),
                            std::end(errors));
                    caseResult.maxAbsoluteError =
                        std::max(
                            caseResult.maxAbsoluteError,
                            local);
                    if (!std::isfinite(local) ||
                        local > 1.0e-5f)
                    {
                        if (caseResult.firstActiveMismatch ==
                            0xFFFF'FFFFu)
                        {
                            caseResult.firstActiveMismatch = i;
                        }
                        ++caseResult.activeMismatchCount;
                    }
                }

                for (std::uint32_t i = active;
                     i < semantic.maxWorkCount.Value();
                     ++i)
                {
                    if (!IsSentinel(mappedOutput[i]))
                    {
                        caseResult.inactiveTailPreserved = false;
                        break;
                    }
                }

                caseResult.outputDigest = base::Sha256(
                    std::as_bytes(std::span(
                        mappedOutput,
                        semantic.maxWorkCount.Value())));

                if (caseResult.activeMismatchCount != 0 ||
                    !caseResult.inactiveTailPreserved)
                {
                    return base::Result<
                        std::vector<CandidateFamilyRunResultV1>,
                        CandidateFamilyExecutionErrorV1>::Failure(
                            Error(
                                "observation",
                                "Candidate family output contract failed."));
                }

                run.cases.push_back(std::move(caseResult));
            }

            base::BinaryWriter identity;
            identity.WriteBytes(LiteralIdentity(
                "SGE4-5.Spiral4.ActualCandidateFamilyExecution.V1"));
            identity.WriteBytes(run.verifiedBindingIdentity);
            identity.WriteBytes(
                run.producerShaderBytecodeDigest);
            identity.WriteBytes(
                run.consumerShaderBytecodeDigest);
            identity.WriteU32(
                static_cast<std::uint32_t>(run.kind));
            identity.WriteU32(run.batchSize);
            run.actualExecutionIdentity =
                base::Sha256(std::move(identity).Take());
            results.push_back(std::move(run));
        }

        activeCount->Unmap(0, nullptr);
        outputReadback->Unmap(0, nullptr);
        batchReadback->Unmap(0, nullptr);

        std::stable_sort(
            results.begin(),
            results.end(),
            [](const CandidateFamilyRunResultV1& a,
               const CandidateFamilyRunResultV1& b)
            {
                return static_cast<std::uint32_t>(a.kind) <
                       static_cast<std::uint32_t>(b.kind) ||
                       (a.kind == b.kind &&
                        a.batchSize < b.batchSize);
            });

        return base::Result<
            std::vector<CandidateFamilyRunResultV1>,
            CandidateFamilyExecutionErrorV1>::Success(
                std::move(results));
    }
    catch (const std::exception& exception)
    {
        return base::Result<
            std::vector<CandidateFamilyRunResultV1>,
            CandidateFamilyExecutionErrorV1>::Failure(
                Error("d3d12/exception", exception.what()));
    }
}
}
