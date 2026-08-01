class ExternalResourceBase : public runtime::IExternalResource
{
public:
    ExternalResourceBase(ComPtr<ID3D12Resource> resource,
                         const void* owner,
                         std::uint64_t epoch,
                         std::uint64_t bytes,
                         std::uint32_t slot,
                         pkg::ResourceKind kind,
                         pkg::Format format,
                         std::uint32_t width,
                         std::uint32_t height,
                         std::uint32_t rowBytes,
                         pkg::ResourceState incoming,
                         pkg::ResourceState outgoing)
        : resource_(std::move(resource)), owner_(owner), epoch_(epoch), bytes_(bytes),
          slot_(slot), kind_(kind), format_(format), width_(width), height_(height),
          rowBytes_(rowBytes), incoming_(incoming), outgoing_(outgoing), current_(incoming) {}
    [[nodiscard]] std::uint64_t DeviceEpoch() const noexcept override { return epoch_; }
    [[nodiscard]] std::uint64_t SizeBytes() const noexcept override { return bytes_; }
    [[nodiscard]] ID3D12Resource* Native() const noexcept { return resource_.Get(); }
    [[nodiscard]] const void* Owner() const noexcept { return owner_; }
    [[nodiscard]] std::uint32_t Slot() const noexcept { return slot_; }
    [[nodiscard]] pkg::ResourceKind Kind() const noexcept { return kind_; }
    [[nodiscard]] pkg::Format Format() const noexcept { return format_; }
    [[nodiscard]] std::uint32_t Width() const noexcept { return width_; }
    [[nodiscard]] std::uint32_t Height() const noexcept { return height_; }
    [[nodiscard]] std::uint32_t RowBytes() const noexcept { return rowBytes_; }
    [[nodiscard]] pkg::ResourceState IncomingState() const noexcept { return incoming_; }
    [[nodiscard]] pkg::ResourceState OutgoingState() const noexcept { return outgoing_; }
    [[nodiscard]] pkg::ResourceState CurrentState() const noexcept { return current_; }
    void SetCurrentState(pkg::ResourceState state) noexcept { current_ = state; }
private:
    ComPtr<ID3D12Resource> resource_;
    const void* owner_ = nullptr;
    std::uint64_t epoch_ = 0;
    std::uint64_t bytes_ = 0;
    std::uint32_t slot_ = package::InvalidIndex;
    pkg::ResourceKind kind_ = pkg::ResourceKind::Buffer;
    pkg::Format format_ = pkg::Format::Unknown;
    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
    std::uint32_t rowBytes_ = 0;
    pkg::ResourceState incoming_{};
    pkg::ResourceState outgoing_{};
    pkg::ResourceState current_{};
};

class ExternalBufferResource final : public ExternalResourceBase
{
public:
    ExternalBufferResource(ComPtr<ID3D12Resource> resource,
                           const void* owner,
                           std::uint64_t epoch,
                           std::uint64_t bytes,
                           std::uint32_t slot,
                           pkg::ResourceState incoming,
                           pkg::ResourceState outgoing)
        : ExternalResourceBase(std::move(resource), owner, epoch, bytes, slot,
              pkg::ResourceKind::Buffer, pkg::Format::Unknown, 0, 0, 0,
              incoming, outgoing) {}
};

class ExternalTexture2DResource final : public ExternalResourceBase
{
public:
    ExternalTexture2DResource(ComPtr<ID3D12Resource> resource,
                              const void* owner,
                              std::uint64_t epoch,
                              std::uint32_t width,
                              std::uint32_t height,
                              std::uint32_t rowBytes,
                              pkg::Format format,
                              std::uint32_t slot,
                              pkg::ResourceState incoming,
                              pkg::ResourceState outgoing)
        : ExternalResourceBase(std::move(resource), owner, epoch,
              static_cast<std::uint64_t>(rowBytes) * height, slot,
              pkg::ResourceKind::Texture2D, format, width, height, rowBytes,
              incoming, outgoing) {}
};

class CompletionToken final : public runtime::ICompletionToken
{
public:
    CompletionToken(ComPtr<ID3D12Fence> fence,
                    std::uint64_t value,
                    std::uint64_t epoch,
                    const void* owner,
                    std::uint32_t slot)
        : fence_(std::move(fence)), value_(value), epoch_(epoch), owner_(owner), slot_(slot) {}
    [[nodiscard]] std::uint64_t DeviceEpoch() const noexcept override { return epoch_; }
    [[nodiscard]] std::uint64_t Value() const noexcept override { return value_; }
    [[nodiscard]] ID3D12Fence* NativeFence() const noexcept { return fence_.Get(); }
    [[nodiscard]] const void* Owner() const noexcept { return owner_; }
    [[nodiscard]] std::uint32_t Slot() const noexcept { return slot_; }
private:
    ComPtr<ID3D12Fence> fence_;
    std::uint64_t value_ = 0;
    std::uint64_t epoch_ = 0;
    const void* owner_ = nullptr;
    std::uint32_t slot_ = package::InvalidIndex;
};
