#include "VerifiedCompositionArtifact.h"

#include "abi2/FrozenCompositionAbi2.h"
#include <algorithm>
#include <utility>

namespace sge4::composition::artifact
{
namespace
{
using VerificationError = verification::VerificationError;

template<class T>
[[nodiscard]] base::Expected<T, VerificationError> Failure(
    std::string stage,
    std::string message)
{
    return base::Failure<T, VerificationError>(
        {std::move(stage), std::move(message)});
}

}

base::Expected<VerifiedFrozenComposition, verification::VerificationError>
CreateVerifiedFrozenComposition(
    std::vector<std::byte> fileBytes,
    base::Digest256 coreDigest,
    base::Digest256 identitySeedDigest,
    base::Digest256 fileDigest,
    std::uint16_t containerFormatMajor,
    std::vector<std::byte> contractBytes,
    std::vector<CanonicalLeafPackage> leaves,
    std::vector<std::byte> verifiedDecisionBytes,
    std::vector<std::byte> verificationCertificateBytes)
{
    auto contractResult = DeserializeCompositionContract(contractBytes);
    if (!contractResult)
        return Failure<VerifiedFrozenComposition>(
            "read/contract", contractResult.error().stage + "：" + contractResult.error().message);

    auto validatedResult = ValidateCompositionContractAgainstLeaves(
        std::move(contractResult).value(), std::move(leaves));
    if (!validatedResult)
        return Failure<VerifiedFrozenComposition>(
            "read/contract-authority",
            validatedResult.error().stage + "：" + validatedResult.error().message);
    auto validated = std::move(validatedResult).value();

    auto rawResult = planning::DeserializeRawCompositionPlan(verifiedDecisionBytes);
    if (!rawResult)
        return Failure<VerifiedFrozenComposition>(
            "read/plan", rawResult.error().stage + "：" + rawResult.error().message);
    auto certificateResult = verification::DeserializeVerificationCertificate(
        verificationCertificateBytes);
    if (!certificateResult)
        return base::Failure<VerifiedFrozenComposition, VerificationError>(
            certificateResult.error());

    auto verifiedResult = verification::VerifyAndSeal(validated, rawResult.value());
    if (!verifiedResult)
        return Failure<VerifiedFrozenComposition>(
            "read/independent-verification",
            verifiedResult.error().stage + "：" + verifiedResult.error().message);
    auto verified = std::move(verifiedResult).value();
    const auto& encoded = certificateResult.value();
    const auto& derived = verified.Certificate();
    if (encoded.schemaVersion != derived.schemaVersion ||
        encoded.algorithm != derived.algorithm ||
        encoded.contractIdentity != derived.contractIdentity ||
        encoded.planIdentity != derived.planIdentity || encoded.seal != derived.seal)
        return Failure<VerifiedFrozenComposition>(
            "read/certificate", "Certificateが検証または実行の契約に違反しています。");

    return base::Success<VerifiedFrozenComposition, VerificationError>(
        VerifiedFrozenComposition(
            std::move(fileBytes), coreDigest, identitySeedDigest, fileDigest,
            containerFormatMajor, std::move(validated), std::move(verified),
            VerifiedFrozenComposition::ConstructionToken{}));
}

base::Expected<VerifiedFrozenComposition, verification::VerificationError>
ReadVerifiedFrozenComposition(std::span<const std::byte> bytes)
{
    return ReadVerifiedFrozenComposition(std::vector<std::byte>(bytes.begin(), bytes.end()));
}

base::Expected<VerifiedFrozenComposition, verification::VerificationError>
ReadVerifiedFrozenComposition(std::vector<std::byte> bytes)
{
    if (bytes.size() < FrozenCompositionAbi2Magic.size() ||
        !std::equal(
            FrozenCompositionAbi2Magic.begin(),
            FrozenCompositionAbi2Magic.end(),
            bytes.begin()))
        return Failure<VerifiedFrozenComposition>(
            "read/container", "Production ReaderはSGE4UNI 2.3だけを受理します。");
    return ReadVerifiedFrozenCompositionAbi2(std::move(bytes));
}
}
