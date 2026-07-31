#pragma once

#include "../193_Level4V2IndependentVerifier/IndependentVerifier.h"

#include <utility>

namespace sge4_5::v2::frozen
{
namespace canonical = sge4_5::v2::canonical;
namespace authority = sge4_5::v2::authority;

class OpaqueFrozenArtifactV1 final
{
public:
    OpaqueFrozenArtifactV1(const OpaqueFrozenArtifactV1&) = default;
    OpaqueFrozenArtifactV1& operator=(const OpaqueFrozenArtifactV1&) = default;

    [[nodiscard]] const canonical::FrozenArtifactDescriptorV1& Descriptor() const noexcept { return descriptor_; }
    [[nodiscard]] const authority::VerificationSealIdentity& SealIdentity() const noexcept { return sealIdentity_; }
    [[nodiscard]] const canonical::WriteSetIdentity& WriteSetIdentityValue() const noexcept { return writeSetIdentity_; }
    [[nodiscard]] const authority::OperationSequenceIdentity& OperationSequenceIdentityValue() const noexcept
    {
        return operationSequenceIdentity_;
    }

private:
    friend class FrozenAuthorityBuilderV1;

    OpaqueFrozenArtifactV1(
        canonical::FrozenArtifactDescriptorV1 descriptor,
        authority::VerificationSealIdentity sealIdentity,
        canonical::WriteSetIdentity writeSetIdentity,
        authority::OperationSequenceIdentity operationSequenceIdentity) noexcept
        : descriptor_(std::move(descriptor)),
          sealIdentity_(std::move(sealIdentity)),
          writeSetIdentity_(std::move(writeSetIdentity)),
          operationSequenceIdentity_(std::move(operationSequenceIdentity))
    {
    }

    canonical::FrozenArtifactDescriptorV1 descriptor_;
    authority::VerificationSealIdentity sealIdentity_;
    canonical::WriteSetIdentity writeSetIdentity_;
    authority::OperationSequenceIdentity operationSequenceIdentity_;
};

class FrozenAuthorityBuilderV1 final
{
public:
    [[nodiscard]] static OpaqueFrozenArtifactV1 Freeze(const authority::VerifiedAuthorityV1& verified);
};
}
