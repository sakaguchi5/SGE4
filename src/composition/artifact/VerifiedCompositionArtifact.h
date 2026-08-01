#pragma once

#include "../verifier/CompositionVerifier.h"

#include <span>
#include <vector>

namespace sge4::composition::artifact
{
class VerifiedFrozenComposition final
{
public:
    VerifiedFrozenComposition(const VerifiedFrozenComposition&) = default;
    VerifiedFrozenComposition(VerifiedFrozenComposition&&) noexcept = default;
    VerifiedFrozenComposition& operator=(const VerifiedFrozenComposition&) = default;
    VerifiedFrozenComposition& operator=(VerifiedFrozenComposition&&) noexcept = default;

    [[nodiscard]] const ValidatedCompositionContract& ValidatedContract() const noexcept { return contract_; }
    [[nodiscard]] const verification::VerifiedCompositionPlan& VerifiedPlan() const noexcept { return plan_; }
    [[nodiscard]] std::span<const std::byte> FileBytes() const noexcept { return fileBytes_; }
    [[nodiscard]] const base::Digest256& CoreDigest() const noexcept { return coreDigest_; }
    [[nodiscard]] const base::Digest256& IdentitySeedDigest() const noexcept { return identitySeedDigest_; }
    [[nodiscard]] const base::Digest256& FileDigest() const noexcept { return fileDigest_; }
    [[nodiscard]] std::uint16_t ContainerFormatMajor() const noexcept { return containerFormatMajor_; }
    [[nodiscard]] std::size_t LeafCount() const noexcept { return contract_.Leaves().size(); }

private:
    struct ConstructionToken final {};
    VerifiedFrozenComposition(
        std::vector<std::byte> fileBytes,
        base::Digest256 coreDigest,
        base::Digest256 identitySeedDigest,
        base::Digest256 fileDigest,
        std::uint16_t containerFormatMajor,
        ValidatedCompositionContract contract,
        verification::VerifiedCompositionPlan plan,
        ConstructionToken)
        : fileBytes_(std::move(fileBytes)), coreDigest_(coreDigest),
          identitySeedDigest_(identitySeedDigest), fileDigest_(fileDigest),
          containerFormatMajor_(containerFormatMajor), contract_(std::move(contract)),
          plan_(std::move(plan)) {}

    std::vector<std::byte> fileBytes_;
    base::Digest256 coreDigest_{};
    base::Digest256 identitySeedDigest_{};
    base::Digest256 fileDigest_{};
    std::uint16_t containerFormatMajor_ = 0;
    ValidatedCompositionContract contract_;
    verification::VerifiedCompositionPlan plan_;

    friend base::Expected<VerifiedFrozenComposition, verification::VerificationError>
    CreateVerifiedFrozenComposition(
        std::vector<std::byte>, base::Digest256, base::Digest256, base::Digest256,
        std::uint16_t, std::vector<std::byte>, std::vector<CanonicalLeafPackage>,
        std::vector<std::byte>, std::vector<std::byte>);
};

// 共通のDecoded Composition生成経路。ABI Readerは固定幅Binaryを検証してから
// Contract／Leaf／Plan／Certificateをこの関数へ渡す。
[[nodiscard]] base::Expected<VerifiedFrozenComposition, verification::VerificationError>
CreateVerifiedFrozenComposition(
    std::vector<std::byte> fileBytes,
    base::Digest256 coreDigest,
    base::Digest256 identitySeedDigest,
    base::Digest256 fileDigest,
    std::uint16_t containerFormatMajor,
    std::vector<std::byte> contractBytes,
    std::vector<CanonicalLeafPackage> leaves,
    std::vector<std::byte> verifiedDecisionBytes,
    std::vector<std::byte> verificationCertificateBytes);

// Production経路は平坦なSGE4UNI 2.6だけを受理する。
// ABI 1.1／SGE4CMP 1.0のReaderはmigration/abi1内へ隔離する。
[[nodiscard]] base::Expected<VerifiedFrozenComposition, verification::VerificationError>
ReadVerifiedFrozenComposition(std::span<const std::byte> bytes);

[[nodiscard]] base::Expected<VerifiedFrozenComposition, verification::VerificationError>
ReadVerifiedFrozenComposition(std::vector<std::byte> bytes);
}
