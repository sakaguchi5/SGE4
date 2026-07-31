class ExternalBufferResource final : public runtime::IExternalResource
{
public:
    ExternalBufferResource(ComPtr<ID3D12Resource> resource,
                           const void* owner,
                           std::uint64_t epoch,
                           std::uint64_t bytes,
                           std::uint32_t slot,
                           pkg::ResourceState incoming,
                           pkg::ResourceState outgoing)
        : resource_(std::move(resource)), owner_(owner), epoch_(epoch), bytes_(bytes),
          slot_(slot), incoming_(incoming), outgoing_(outgoing), current_(incoming) {}
    [[nodiscard]] std::uint64_t DeviceEpoch() const noexcept override { return epoch_; }
    [[nodiscard]] std::uint64_t SizeBytes() const noexcept override { return bytes_; }
    [[nodiscard]] ID3D12Resource* Native() const noexcept { return resource_.Get(); }
    [[nodiscard]] const void* Owner() const noexcept { return owner_; }
    [[nodiscard]] std::uint32_t Slot() const noexcept { return slot_; }
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
    pkg::ResourceState incoming_{};
    pkg::ResourceState outgoing_{};
    pkg::ResourceState current_{};
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

