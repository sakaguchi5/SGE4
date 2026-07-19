#include "../14_D3D12Backend/D3D12Backend.h"

#include <array>
#include <iostream>

int main()
{
    using Op = sge4_5::package::d3d12_v13::D3D12OperationCode;
    constexpr std::array required = {
        Op::CreateDescriptorHeaps, Op::CreateResource, Op::UploadBuffer,
        Op::InitializeState, Op::VerifyBufferContents, Op::UploadTexture, Op::VerifyTextureContents,
        Op::CreateRootSignature, Op::CreateGraphicsPipeline, Op::CreateComputePipeline,
        Op::AcquireSurfaceImage, Op::AcquireExternal, Op::WaitExternal, Op::ApplyDynamicData, Op::BeginQueueBatch, Op::Transition,
        Op::ActivateAlias, Op::ExecuteCompute, Op::ExecuteCopy, Op::ExecuteRaster, Op::EndQueueBatch, Op::SignalQueue, Op::WaitQueue, Op::WaitTemporal, Op::ReleaseExternal, Op::PresentSurface
    };
    for (auto op : required)
        if (!sge4_5::d3d12::D3D12Backend::SupportsOperation(op)) { std::cerr << "required operation unsupported\n"; return 1; }
    if (sge4_5::d3d12::D3D12Backend::SupportsOperation(static_cast<Op>(0x0002ffff))) return 2;
    std::cout << "D3D12 slice-13 operation and recovery-interface conformance tests passed.\n";
    return 0;
}
