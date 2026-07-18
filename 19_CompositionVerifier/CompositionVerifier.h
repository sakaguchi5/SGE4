#pragma once

#include "../00_Foundation/Result.h"
#include "../00_Foundation/Sha256.h"
#include "../16_FrozenCompositionArtifact/FrozenComposition.h"
#include "../17_CompositionContract/CompositionContract.h"
#include "../18_CompositionPlan/CompositionPlan.h"

#include <span>
#include <string>
#include <utility>
#include <vector>

namespace sge4::composition::canonical::verification
{
inline constexpr std::uint32_t CompositionVerifierSchemaVersion = 1;

struct VerificationError final
{
    std::string stage;
    std::string message;
};

struct VerificationCertificate final
{
    std::uint32_t schemaVersion = CompositionVerifierSchemaVersion;
    std::uint32_t algorithm = 1;
    base::Digest256 contractIdentity{};
    base::Digest256 planIdentity{};
    base::Digest256 seal{};
};

class VerifiedCompositionPlan final
{
public:
    VerifiedCompositionPlan(const VerifiedCompositionPlan&) = default;
    VerifiedCompositionPlan(VerifiedCompositionPlan&&) noexcept = default;
    VerifiedCompositionPlan& operator=(const VerifiedCompositionPlan&) = default;
    VerifiedCompositionPlan& operator=(VerifiedCompositionPlan&&) noexcept = default;

    [[nodiscard]] const planning::RawCompositionPlan& Plan() const noexcept { return plan_; }
    [[nodiscard]] const VerificationCertificate& Certificate() const noexcept { return certificate_; }

private:
    struct ConstructionToken final {};
    VerifiedCompositionPlan(
        planning::RawCompositionPlan plan,
        VerificationCertificate certificate,
        ConstructionToken)
        : plan_(std::move(plan)), certificate_(certificate) {}

    planning::RawCompositionPlan plan_;
    VerificationCertificate certificate_;

    friend base::Result<VerifiedCompositionPlan, VerificationError>
    VerifyAndSeal(const ValidatedCompositionContract&, const planning::RawCompositionPlan&);
};

class VerifiedFrozenComposition final
{
public:
    VerifiedFrozenComposition(const VerifiedFrozenComposition&) = default;
    VerifiedFrozenComposition(VerifiedFrozenComposition&&) noexcept = default;
    VerifiedFrozenComposition& operator=(const VerifiedFrozenComposition&) = default;
    VerifiedFrozenComposition& operator=(VerifiedFrozenComposition&&) noexcept = default;

    [[nodiscard]] const ::sge4::composition::FrozenComposition& Artifact() const noexcept { return artifact_; }
    [[nodiscard]] const ValidatedCompositionContract& ValidatedContract() const noexcept { return contract_; }
    [[nodiscard]] const VerifiedCompositionPlan& VerifiedPlan() const noexcept { return plan_; }

private:
    struct ConstructionToken final {};
    VerifiedFrozenComposition(
        ::sge4::composition::FrozenComposition artifact,
        ValidatedCompositionContract contract,
        VerifiedCompositionPlan plan,
        ConstructionToken)
        : artifact_(std::move(artifact)), contract_(std::move(contract)), plan_(std::move(plan)) {}

    ::sge4::composition::FrozenComposition artifact_;
    ValidatedCompositionContract contract_;
    VerifiedCompositionPlan plan_;

    friend base::Result<VerifiedFrozenComposition, VerificationError>
    ReadVerifiedFrozenComposition(std::vector<std::byte>);
};

[[nodiscard]] base::Digest256
ComputeVerifierSeal(
    const base::Digest256& contractIdentity,
    const base::Digest256& rawPlanIdentity);

[[nodiscard]] base::Result<VerifiedCompositionPlan, VerificationError>
VerifyAndSeal(
    const ValidatedCompositionContract& contract,
    const planning::RawCompositionPlan& rawPlan);

[[nodiscard]] base::Result<void, VerificationError>
ValidateVerifiedPlan(
    const ValidatedCompositionContract& contract,
    const VerifiedCompositionPlan& verified);

[[nodiscard]] std::vector<std::byte>
SerializeVerificationCertificate(const VerificationCertificate& certificate);

[[nodiscard]] base::Result<VerificationCertificate, VerificationError>
DeserializeVerificationCertificate(std::span<const std::byte> bytes);

// The only canonical freeze entry point. No RawCompositionPlan overload exists.
[[nodiscard]] base::Result<std::vector<std::byte>, VerificationError>
FreezeVerifiedComposition(
    const ValidatedCompositionContract& contract,
    const VerifiedCompositionPlan& verified);

[[nodiscard]] base::Result<VerifiedFrozenComposition, VerificationError>
ReadVerifiedFrozenComposition(std::span<const std::byte> bytes);

[[nodiscard]] base::Result<VerifiedFrozenComposition, VerificationError>
ReadVerifiedFrozenComposition(std::vector<std::byte> bytes);
}
