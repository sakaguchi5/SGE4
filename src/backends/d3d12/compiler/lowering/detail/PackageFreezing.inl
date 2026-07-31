base::Expected<CompileOutput, CompileError> FreezePackageStage(LoweredPackageStage lowered)
{
    auto packageBytes = pkg::BuildFrozenPackage(lowered.description);
    if (!packageBytes)
        return Failure<CompileOutput>("package-serialization", packageBytes.error().message);
    auto verified = package::PackageReader::Read(packageBytes.value());
    if (!verified)
        return Failure<CompileOutput>("package-validation", verified.error().message);
    auto decoded = pkg::D3D12PackageView::Decode(verified.value());
    if (!decoded)
        return Failure<CompileOutput>("package-schema-validation", decoded.error().message);

    CompileOutput output;
    output.executionDigestHex = base::ToHex(verified.value().ExecutionDigest());
    output.packageBytes = std::move(packageBytes).value();
    return base::Success<CompileOutput, CompileError>(std::move(output));
}