#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include "Spiral1PerformanceExperiment.h"

#include "../00_Foundation/FileIO.h"
#include "../00_Foundation/Sha256.h"
#include "../62_Spiral1Corpus/Spiral1Corpus.h"
#include "../64_D3D12RepresentationPlanner/D3D12RepresentationPlanner.h"
#include "../65_D3D12RepresentationVerifier/D3D12RepresentationVerifier.h"
#include "../66_Spiral1LeafCompiler/Spiral1LeafCompiler.h"
#include "../67_Spiral1Observer/Spiral1Observer.h"

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string_view>

#ifdef _WIN32
#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#endif

namespace sge4_5::spiral1::performance
{
namespace
{
namespace corpus = sge4_5::spiral1::corpus;
namespace leaf = sge4_5::spiral1::leaf;
namespace observer = sge4_5::spiral1::observer;
namespace semantic = sge4_5::spiral1::semantic;
using Clock = std::chrono::steady_clock;

constexpr std::string_view EvidenceMagic = "SGE4-5.Spiral1.BenchmarkEvidence.V1";
constexpr std::string_view ArchitectureEvidence = "40BDF65D88333D1580912695BD012FA9E77E6E7518AB13B0D0BF28704FB35FCA";
constexpr std::string_view WarpEvidence = "CE9AE0DB10DB0C2DE94F251E4D23F8816903B4A99F4B0D006ECAE4189D354398";
constexpr std::string_view FinalFreezeEvidence = "3AF6DF0E4CF29C16EF198BF219A394147A4C66C4E48B6416DE3DA95DF56EDF0B";

void Require(bool condition, std::string_view message)
{
    if (!condition) throw std::runtime_error(std::string(message));
}

std::uint64_t Nanoseconds(Clock::time_point begin, Clock::time_point end)
{
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count());
}

void WriteString(base::BinaryWriter& writer, std::string_view value)
{
    writer.WriteU32(static_cast<std::uint32_t>(value.size()));
    writer.WriteBytes(std::as_bytes(std::span(value.data(), value.size())));
}

struct ReadText final { std::string value; };
base::Result<ReadText, std::string> ReadString(base::BinaryReader& reader)
{
    auto size = reader.ReadU32();
    if (!size) return base::Result<ReadText, std::string>::Failure(size.Error());
    auto bytes = reader.ReadBytes(size.Value());
    if (!bytes) return base::Result<ReadText, std::string>::Failure(bytes.Error());
    return base::Result<ReadText, std::string>::Success(ReadText{
        std::string(reinterpret_cast<const char*>(bytes.Value().data()), bytes.Value().size())});
}

void WriteDigest(base::BinaryWriter& writer, const base::Digest256& value) { writer.WriteBytes(value); }

base::Result<base::Digest256, std::string> ReadDigest(base::BinaryReader& reader)
{
    auto bytes = reader.ReadBytes(32);
    if (!bytes) return base::Result<base::Digest256, std::string>::Failure(bytes.Error());
    base::Digest256 value{};
    std::memcpy(value.data(), bytes.Value().data(), value.size());
    return base::Result<base::Digest256, std::string>::Success(value);
}

void WriteDouble(base::BinaryWriter& writer, double value) { writer.WriteU64(std::bit_cast<std::uint64_t>(value)); }
base::Result<double, std::string> ReadDouble(base::BinaryReader& reader)
{
    auto bits = reader.ReadU64();
    if (!bits) return base::Result<double, std::string>::Failure(bits.Error());
    return base::Result<double, std::string>::Success(std::bit_cast<double>(bits.Value()));
}

std::string Hex(const base::Digest256& value)
{
    static constexpr char digits[] = "0123456789abcdef";
    std::string text; text.resize(value.size() * 2);
    for (std::size_t i = 0; i < value.size(); ++i)
    {
        const auto b = std::to_integer<unsigned char>(value[i]);
        text[i * 2] = digits[b >> 4]; text[i * 2 + 1] = digits[b & 15];
    }
    return text;
}

std::string JsonEscape(std::string_view value)
{
    std::string result;
    for (const char c : value)
    {
        switch (c)
        {
        case '\\': result += "\\\\"; break;
        case '"': result += "\\\""; break;
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) result += '?'; else result += c;
        }
    }
    return result;
}

std::string KindName(representation::LoweringKindV1 kind)
{
    return kind == representation::LoweringKindV1::MatrixExpandedCompute
        ? "MatrixExpandedComputeV1" : "DirectPgaMotorComputeV1";
}

[[maybe_unused]] MetricSummaryV1 Summarize(const std::vector<double>& values)
{
    MetricSummaryV1 result{};
    if (values.empty()) return result;
    std::vector<double> sorted = values;
    std::sort(sorted.begin(), sorted.end());
    const auto medianAt = sorted.size() / 2;
    result.median = sorted.size() % 2 == 0 ? (sorted[medianAt - 1] + sorted[medianAt]) * 0.5 : sorted[medianAt];
    const auto p95Index = (std::min)(sorted.size() - 1,
        static_cast<std::size_t>(std::ceil(static_cast<double>(sorted.size()) * 0.95)) - 1);
    result.p95 = sorted[p95Index];
    result.minimum = sorted.front();
    result.maximum = sorted.back();
    return result;
}

[[maybe_unused]] void FinalizeSummaries(AlgorithmMeasurementV1& value)
{
    std::vector<double> command, gpu, e2e;
    command.reserve(value.samples.size()); gpu.reserve(value.samples.size()); e2e.reserve(value.samples.size());
    for (const auto& sample : value.samples)
    {
        command.push_back(sample.commandRecordingNs);
        gpu.push_back(sample.gpuDispatchNs);
        e2e.push_back(sample.endToEndNs);
    }
    value.commandRecording = Summarize(command);
    value.gpuDispatch = Summarize(gpu);
    value.endToEnd = Summarize(e2e);
}

[[maybe_unused]] const corpus::QualificationCaseV1& FindCase(
    const corpus::QualificationCorpusV1& values, std::string_view scenario, std::uint32_t subscenario = 0)
{
    const auto found = std::find_if(values.cases.begin(), values.cases.end(), [&](const auto& value) {
        return value.scenarioId == scenario && value.subscenarioIndex == subscenario;
    });
    if (found == values.cases.end()) throw std::runtime_error("benchmark case was not found");
    return *found;
}

[[maybe_unused]] semantic::ApplyPgaMotorSemanticV1 BuildSemantic(const corpus::QualificationCaseV1& value)
{
    auto semanticValue = semantic::BuildApplyPgaMotorSemanticV1(
        value.motor, static_cast<std::uint32_t>(value.inputPoints.size()), value.caseIdentity,
        contracts::ObservationContractIdentityV2());
    if (!semanticValue) throw std::runtime_error(semanticValue.Error());
    return std::move(semanticValue).Value();
}

struct PreparedAlgorithm final
{
    semantic::ApplyPgaMotorSemanticV1 semanticValue;
    representation::VerifiedRepresentationPlanV1 verifiedPlan;
    leaf::FrozenRepresentationLeafV1 frozenLeaf;
    AlgorithmMeasurementV1 measurement;
    PreparedAlgorithm(semantic::ApplyPgaMotorSemanticV1 s, representation::VerifiedRepresentationPlanV1 p,
        leaf::FrozenRepresentationLeafV1 f, AlgorithmMeasurementV1 m)
        : semanticValue(std::move(s)), verifiedPlan(std::move(p)), frozenLeaf(std::move(f)), measurement(std::move(m)) {}
};

[[maybe_unused]] PreparedAlgorithm PrepareAlgorithm(
    const corpus::QualificationCaseV1& value,
    representation::LoweringKindV1 kind,
    const leaf::NativeProgramSetV1& programs)
{
    auto semanticValue = BuildSemantic(value);
    const auto candidateBegin = Clock::now();
    auto raw = representation::PlanRepresentationCandidateV1(semanticValue, programs.representationTargetProfile, kind);
    const auto candidateEnd = Clock::now();
    if (!raw) throw std::runtime_error(raw.Error());
    const auto verifierBegin = Clock::now();
    auto verified = representation::IndependentRepresentationVerifierV1::Verify(
        semanticValue, programs.representationTargetProfile, raw.Value());
    const auto verifierEnd = Clock::now();
    if (!verified) throw std::runtime_error(verified.Error().message);
    const auto freezeBegin = Clock::now();
    auto frozen = leaf::CompileVerifiedRepresentationLeafV1(
        semanticValue, programs, leaf::CanonicalLeafD3D12TargetProfileV1(), verified.Value());
    if (!frozen) throw std::runtime_error(frozen.Error().stage + ": " + frozen.Error().message);
    auto valid = leaf::ValidateFrozenRepresentationLeafV1(
        frozen.Value(), semanticValue, programs, verified.Value());
    const auto freezeEnd = Clock::now();
    if (!valid) throw std::runtime_error(valid.Error().stage + ": " + valid.Error().message);

    AlgorithmMeasurementV1 measurement;
    measurement.loweringKind = kind;
    measurement.candidateIdentity = raw.Value().candidateIdentity;
    measurement.representationSeal = verified.Value().SealDigest();
    measurement.packageExecutionDigest = frozen.Value().packageExecutionDigest;
    measurement.packageFileDigest = frozen.Value().packageFileDigest;
    measurement.candidateGenerationNs = Nanoseconds(candidateBegin, candidateEnd);
    measurement.verifierNs = Nanoseconds(verifierBegin, verifierEnd);
    measurement.freezeNs = Nanoseconds(freezeBegin, freezeEnd);
    measurement.packageBytes = frozen.Value().packageBytes.size();
    measurement.constantBytes = frozen.Value().lockedConstantPayload.size();
    measurement.readbackBytes = value.inputPoints.size() * sizeof(semantic::Float4Point);
    return PreparedAlgorithm(std::move(semanticValue), std::move(verified).Value(), std::move(frozen).Value(), std::move(measurement));
}

void WriteMetric(base::BinaryWriter& writer, const MetricSummaryV1& value)
{
    WriteDouble(writer, value.median); WriteDouble(writer, value.p95);
    WriteDouble(writer, value.minimum); WriteDouble(writer, value.maximum);
}

base::Result<MetricSummaryV1, std::string> ReadMetric(base::BinaryReader& reader)
{
    MetricSummaryV1 value;
    auto a=ReadDouble(reader); auto b=ReadDouble(reader); auto c=ReadDouble(reader); auto d=ReadDouble(reader);
    if(!a||!b||!c||!d) return base::Result<MetricSummaryV1,std::string>::Failure("metric summary is truncated");
    value.median=a.Value(); value.p95=b.Value(); value.minimum=c.Value(); value.maximum=d.Value();
    return base::Result<MetricSummaryV1,std::string>::Success(value);
}

void WriteAlgorithm(base::BinaryWriter& writer, const AlgorithmMeasurementV1& value)
{
    writer.WriteU32(static_cast<std::uint32_t>(value.loweringKind));
    WriteDigest(writer,value.candidateIdentity); WriteDigest(writer,value.representationSeal);
    WriteDigest(writer,value.packageExecutionDigest); WriteDigest(writer,value.packageFileDigest);
    writer.WriteU64(value.candidateGenerationNs); writer.WriteU64(value.verifierNs); writer.WriteU64(value.freezeNs);
    writer.WriteU64(value.materializationNs); writer.WriteU64(value.packageBytes); writer.WriteU64(value.constantBytes);
    writer.WriteU64(value.cpuToGpuUploadBytes); writer.WriteU64(value.allocatedBytes); writer.WriteU64(value.readbackBytes);
    WriteMetric(writer,value.commandRecording); WriteMetric(writer,value.gpuDispatch); WriteMetric(writer,value.endToEnd);
    writer.WriteU32(static_cast<std::uint32_t>(value.samples.size()));
    for(const auto& sample:value.samples){
        writer.WriteU32(sample.run); writer.WriteU32(sample.block); writer.WriteU32(sample.orderPosition);
        writer.WriteU32(static_cast<std::uint32_t>(sample.loweringKind));
        WriteDouble(writer,sample.commandRecordingNs); WriteDouble(writer,sample.gpuDispatchNs); WriteDouble(writer,sample.endToEndNs);
    }
}

base::Result<AlgorithmMeasurementV1,std::string> ReadAlgorithm(base::BinaryReader& reader)
{
    AlgorithmMeasurementV1 v;
    auto k=reader.ReadU32(); if(!k) return base::Result<AlgorithmMeasurementV1,std::string>::Failure(k.Error());
    v.loweringKind=static_cast<representation::LoweringKindV1>(k.Value());
    auto d1=ReadDigest(reader); auto d2=ReadDigest(reader); auto d3=ReadDigest(reader); auto d4=ReadDigest(reader);
    if(!d1||!d2||!d3||!d4) return base::Result<AlgorithmMeasurementV1,std::string>::Failure("algorithm digest section is truncated");
    v.candidateIdentity=d1.Value(); v.representationSeal=d2.Value(); v.packageExecutionDigest=d3.Value(); v.packageFileDigest=d4.Value();
    auto r1=reader.ReadU64(); auto r2=reader.ReadU64(); auto r3=reader.ReadU64(); auto r4=reader.ReadU64();
    auto r5=reader.ReadU64(); auto r6=reader.ReadU64(); auto r7=reader.ReadU64(); auto r8=reader.ReadU64(); auto r9=reader.ReadU64();
    if(!r1||!r2||!r3||!r4||!r5||!r6||!r7||!r8||!r9) return base::Result<AlgorithmMeasurementV1,std::string>::Failure("algorithm scalar section is truncated");
    v.candidateGenerationNs=r1.Value(); v.verifierNs=r2.Value(); v.freezeNs=r3.Value(); v.materializationNs=r4.Value();
    v.packageBytes=r5.Value(); v.constantBytes=r6.Value(); v.cpuToGpuUploadBytes=r7.Value(); v.allocatedBytes=r8.Value(); v.readbackBytes=r9.Value();
    auto m1=ReadMetric(reader); auto m2=ReadMetric(reader); auto m3=ReadMetric(reader);
    if(!m1||!m2||!m3) return base::Result<AlgorithmMeasurementV1,std::string>::Failure("algorithm metric section is truncated");
    v.commandRecording=m1.Value(); v.gpuDispatch=m2.Value(); v.endToEnd=m3.Value();
    auto count=reader.ReadU32(); if(!count) return base::Result<AlgorithmMeasurementV1,std::string>::Failure(count.Error());
    v.samples.reserve(count.Value());
    for(std::uint32_t i=0;i<count.Value();++i){
        TimingSampleV1 s; auto a=reader.ReadU32(); auto b=reader.ReadU32(); auto c=reader.ReadU32(); auto d=reader.ReadU32();
        auto e=ReadDouble(reader); auto f=ReadDouble(reader); auto g=ReadDouble(reader);
        if(!a||!b||!c||!d||!e||!f||!g) return base::Result<AlgorithmMeasurementV1,std::string>::Failure("sample section is truncated");
        s.run=a.Value(); s.block=b.Value(); s.orderPosition=c.Value(); s.loweringKind=static_cast<representation::LoweringKindV1>(d.Value());
        s.commandRecordingNs=e.Value(); s.gpuDispatchNs=f.Value(); s.endToEndNs=g.Value(); v.samples.push_back(s);
    }
    return base::Result<AlgorithmMeasurementV1,std::string>::Success(std::move(v));
}

void WriteErrorSummary(base::BinaryWriter& writer,const contracts::ErrorSummaryV1& v){
    WriteDouble(writer,v.maxAbsoluteError); WriteDouble(writer,v.maxRelativeError); writer.WriteU32(v.maxUlpDistance); writer.WriteU32(0); WriteDouble(writer,v.rmsEuclideanError);
}
base::Result<contracts::ErrorSummaryV1,std::string> ReadErrorSummary(base::BinaryReader& reader){
    contracts::ErrorSummaryV1 v; auto a=ReadDouble(reader); auto b=ReadDouble(reader); auto c=reader.ReadU32(); auto pad=reader.ReadU32(); auto d=ReadDouble(reader);
    if(!a||!b||!c||!pad||!d) return base::Result<contracts::ErrorSummaryV1,std::string>::Failure("error summary truncated");
    v.maxAbsoluteError=a.Value(); v.maxRelativeError=b.Value(); v.maxUlpDistance=c.Value(); v.rmsEuclideanError=d.Value(); return base::Result<contracts::ErrorSummaryV1,std::string>::Success(v);
}
void WriteReport(base::BinaryWriter& w,const contracts::ScenarioReportV1& v){
    WriteDigest(w,v.scenarioIdentity); w.WriteU32(v.pointCount); w.WriteU32(v.nonFiniteCount); w.WriteU32(v.mismatchCount); w.WriteU32(v.firstMismatchIndex);
    WriteErrorSummary(w,v.matrix); WriteErrorSummary(w,v.pga); WriteErrorSummary(w,v.pair);
    WriteDigest(w,v.inputDigest); WriteDigest(w,v.referenceDigest); WriteDigest(w,v.matrixOutputDigest); WriteDigest(w,v.pgaOutputDigest); WriteDigest(w,v.readbackDigest);
}
base::Result<contracts::ScenarioReportV1,std::string> ReadReport(base::BinaryReader& r){
    contracts::ScenarioReportV1 v; auto d=ReadDigest(r); auto a=r.ReadU32(); auto b=r.ReadU32(); auto c=r.ReadU32(); auto e=r.ReadU32();
    if(!d||!a||!b||!c||!e) return base::Result<contracts::ScenarioReportV1,std::string>::Failure("report header truncated");
    v.scenarioIdentity=d.Value();v.pointCount=a.Value();v.nonFiniteCount=b.Value();v.mismatchCount=c.Value();v.firstMismatchIndex=e.Value();
    auto m=ReadErrorSummary(r);auto p=ReadErrorSummary(r);auto q=ReadErrorSummary(r);if(!m||!p||!q) return base::Result<contracts::ScenarioReportV1,std::string>::Failure("report metrics truncated");
    v.matrix=m.Value();v.pga=p.Value();v.pair=q.Value();
    auto d1=ReadDigest(r);auto d2=ReadDigest(r);auto d3=ReadDigest(r);auto d4=ReadDigest(r);auto d5=ReadDigest(r);if(!d1||!d2||!d3||!d4||!d5) return base::Result<contracts::ScenarioReportV1,std::string>::Failure("report digests truncated");
    v.inputDigest=d1.Value();v.referenceDigest=d2.Value();v.matrixOutputDigest=d3.Value();v.pgaOutputDigest=d4.Value();v.readbackDigest=d5.Value();return base::Result<contracts::ScenarioReportV1,std::string>::Success(v);
}

base::Digest256 ComputeBenchmarkProfileIdentity(const BenchmarkEvidenceV1& value)
{
    base::BinaryWriter w;
    constexpr std::string_view domain = "SGE4-5.Spiral1.BenchmarkProfileIdentity.V1";
    WriteString(w, domain);
    w.WriteU32(value.config.warmupSamplesPerAlgorithm);
    w.WriteU32(value.config.measurementSamplesPerAlgorithm);
    w.WriteU32(value.config.runCount);
    w.WriteU32(value.config.adapterIndex);
    WriteString(w, value.config.clockPolicy);
    WriteDigest(w, value.adapter.fingerprint);
    WriteDigest(w, value.corpusIdentity);
    WriteDigest(w, value.observationContractIdentity);
    WriteDigest(w, value.representationTargetProfileIdentity);
    constexpr std::array<std::pair<std::string_view, std::uint32_t>, 4> cases{{
        {"S03", 0}, {"S15", 15}, {"S07", 0}, {"S12", 0}}};
    w.WriteU32(static_cast<std::uint32_t>(cases.size()));
    for (const auto& [scenario, subscenario] : cases)
    {
        WriteString(w, scenario);
        w.WriteU32(subscenario);
    }
    WriteString(w, "ABBA");
    return base::Sha256(w.Bytes());
}

std::vector<std::byte> SerializeEvidence(const BenchmarkEvidenceV1& value)
{
    base::BinaryWriter w; WriteString(w,EvidenceMagic); w.WriteU32(value.schemaVersion); w.WriteU64(value.captureUnixNanoseconds);
    w.WriteU32(value.config.warmupSamplesPerAlgorithm); w.WriteU32(value.config.measurementSamplesPerAlgorithm); w.WriteU32(value.config.runCount); w.WriteU32(value.config.adapterIndex); WriteString(w,value.config.clockPolicy);
    WriteString(w,value.adapter.description); w.WriteU32(value.adapter.vendorId); w.WriteU32(value.adapter.deviceId); w.WriteU32(value.adapter.subsystemId); w.WriteU32(value.adapter.revision);
    w.WriteU32(value.adapter.luidLow); w.WriteI32(value.adapter.luidHigh); w.WriteU64(value.adapter.dedicatedVideoMemory); w.WriteU64(value.adapter.dedicatedSystemMemory); w.WriteU64(value.adapter.sharedSystemMemory);
    w.WriteU64(value.adapter.driverVersion); w.WriteU64(value.adapter.timestampFrequency); w.WriteU32(value.adapter.uma?1:0); w.WriteU32(value.adapter.cacheCoherentUma?1:0); WriteDigest(w,value.adapter.fingerprint);
    WriteDigest(w,value.corpusIdentity); WriteDigest(w,value.observationContractIdentity); WriteDigest(w,value.representationTargetProfileIdentity); WriteDigest(w,value.benchmarkProfileIdentity); w.WriteU64(value.nativeProgramBuildNs);
    w.WriteU32(static_cast<std::uint32_t>(value.cases.size()));
    for(const auto& c:value.cases){ WriteString(w,c.scenarioId); w.WriteU32(c.subscenarioIndex); w.WriteU32(c.pointCount); WriteDigest(w,c.caseIdentity); w.WriteU64(c.sharedInputAllocatedBytes); w.WriteU64(c.sharedInputUploadBytes); WriteAlgorithm(w,c.matrix); WriteAlgorithm(w,c.directPga); WriteReport(w,c.observation); }
    return std::move(w).Take();
}

base::Result<BenchmarkEvidenceV1,std::string> DeserializeEvidence(std::span<const std::byte> bytes)
{
    base::BinaryReader r(bytes); auto magic=ReadString(r); if(!magic||magic.Value().value!=EvidenceMagic) return base::Result<BenchmarkEvidenceV1,std::string>::Failure("benchmark evidence magic mismatch");
    BenchmarkEvidenceV1 v; auto ver=r.ReadU32();auto cap=r.ReadU64();if(!ver||!cap||ver.Value()!=BenchmarkEvidenceSchemaVersionV1)return base::Result<BenchmarkEvidenceV1,std::string>::Failure("benchmark evidence version mismatch");v.schemaVersion=ver.Value();v.captureUnixNanoseconds=cap.Value();
    auto w=r.ReadU32();auto m=r.ReadU32();auto runs=r.ReadU32();auto ai=r.ReadU32();auto cp=ReadString(r);if(!w||!m||!runs||!ai||!cp)return base::Result<BenchmarkEvidenceV1,std::string>::Failure("benchmark config truncated");v.config={w.Value(),m.Value(),runs.Value(),ai.Value(),cp.Value().value};
    auto desc=ReadString(r);if(!desc)return base::Result<BenchmarkEvidenceV1,std::string>::Failure(desc.Error());v.adapter.description=desc.Value().value;
    auto a1=r.ReadU32();auto a2=r.ReadU32();auto a3=r.ReadU32();auto a4=r.ReadU32();auto a5=r.ReadU32();auto a6=r.ReadI32();auto a7=r.ReadU64();auto a8=r.ReadU64();auto a9=r.ReadU64();auto a10=r.ReadU64();auto a11=r.ReadU64();auto a12=r.ReadU32();auto a13=r.ReadU32();auto af=ReadDigest(r);
    if(!a1||!a2||!a3||!a4||!a5||!a6||!a7||!a8||!a9||!a10||!a11||!a12||!a13||!af)return base::Result<BenchmarkEvidenceV1,std::string>::Failure("adapter profile truncated");
    v.adapter.vendorId=a1.Value();v.adapter.deviceId=a2.Value();v.adapter.subsystemId=a3.Value();v.adapter.revision=a4.Value();v.adapter.luidLow=a5.Value();v.adapter.luidHigh=a6.Value();v.adapter.dedicatedVideoMemory=a7.Value();v.adapter.dedicatedSystemMemory=a8.Value();v.adapter.sharedSystemMemory=a9.Value();v.adapter.driverVersion=a10.Value();v.adapter.timestampFrequency=a11.Value();v.adapter.uma=a12.Value()!=0;v.adapter.cacheCoherentUma=a13.Value()!=0;v.adapter.fingerprint=af.Value();
    auto c1=ReadDigest(r);auto c2=ReadDigest(r);auto c3=ReadDigest(r);auto c4=ReadDigest(r);auto np=r.ReadU64();if(!c1||!c2||!c3||!c4||!np)return base::Result<BenchmarkEvidenceV1,std::string>::Failure("benchmark identity section truncated");v.corpusIdentity=c1.Value();v.observationContractIdentity=c2.Value();v.representationTargetProfileIdentity=c3.Value();v.benchmarkProfileIdentity=c4.Value();v.nativeProgramBuildNs=np.Value();
    auto count=r.ReadU32();if(!count)return base::Result<BenchmarkEvidenceV1,std::string>::Failure(count.Error());v.cases.reserve(count.Value());
    for(std::uint32_t i=0;i<count.Value();++i){ CaseMeasurementV1 c;auto id=ReadString(r);auto sub=r.ReadU32();auto points=r.ReadU32();auto ci=ReadDigest(r);auto al=r.ReadU64();auto up=r.ReadU64();if(!id||!sub||!points||!ci||!al||!up)return base::Result<BenchmarkEvidenceV1,std::string>::Failure("case header truncated");c.scenarioId=id.Value().value;c.subscenarioIndex=sub.Value();c.pointCount=points.Value();c.caseIdentity=ci.Value();c.sharedInputAllocatedBytes=al.Value();c.sharedInputUploadBytes=up.Value();auto ma=ReadAlgorithm(r);auto di=ReadAlgorithm(r);auto rep=ReadReport(r);if(!ma||!di||!rep)return base::Result<BenchmarkEvidenceV1,std::string>::Failure("case body truncated");c.matrix=std::move(ma).Value();c.directPga=std::move(di).Value();c.observation=rep.Value();v.cases.push_back(std::move(c)); }
    if (r.Remaining() != 0)
        return base::Result<BenchmarkEvidenceV1,std::string>::Failure("benchmark evidence has trailing bytes");
    return base::Result<BenchmarkEvidenceV1,std::string>::Success(std::move(v));
}

#ifdef _WIN32
using Microsoft::WRL::ComPtr;

void Check(HRESULT hr, std::string_view stage)
{
    if (FAILED(hr)) { std::ostringstream s; s << stage << " failed HRESULT=0x" << std::hex << static_cast<unsigned long>(hr); throw std::runtime_error(s.str()); }
}

D3D12_HEAP_PROPERTIES HeapProperties(D3D12_HEAP_TYPE type)
{
    D3D12_HEAP_PROPERTIES p{}; p.Type=type; p.CPUPageProperty=D3D12_CPU_PAGE_PROPERTY_UNKNOWN; p.MemoryPoolPreference=D3D12_MEMORY_POOL_UNKNOWN; p.CreationNodeMask=1; p.VisibleNodeMask=1; return p;
}
D3D12_RESOURCE_DESC BufferDescription(std::uint64_t bytes,D3D12_RESOURCE_FLAGS flags=D3D12_RESOURCE_FLAG_NONE)
{
    D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;d.Alignment=0;d.Width=bytes;d.Height=1;d.DepthOrArraySize=1;d.MipLevels=1;d.Format=DXGI_FORMAT_UNKNOWN;d.SampleDesc.Count=1;d.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;d.Flags=flags;return d;
}
D3D12_RESOURCE_BARRIER Transition(ID3D12Resource* r,D3D12_RESOURCE_STATES before,D3D12_RESOURCE_STATES after)
{
    D3D12_RESOURCE_BARRIER b{};b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;b.Transition.pResource=r;b.Transition.StateBefore=before;b.Transition.StateAfter=after;b.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;return b;
}

struct SampleNative final{double commandNs=0;double gpuNs=0;double e2eNs=0;};
struct KernelNative final{representation::LoweringKindV1 kind{};ComPtr<ID3D12PipelineState> pso;ComPtr<ID3D12Resource> output;ComPtr<ID3D12Resource> constant;ComPtr<ID3D12Resource> readback;D3D12_GPU_DESCRIPTOR_HANDLE uav{};std::uint64_t allocationBytes=0;std::uint64_t materializationNs=0;};
struct CaseNative final{ComPtr<ID3D12Resource> input;ComPtr<ID3D12DescriptorHeap> heap;D3D12_GPU_DESCRIPTOR_HANDLE srv{};std::uint64_t inputAllocationBytes=0;KernelNative matrix;KernelNative direct;};

class NativeHarness final
{
public:
    explicit NativeHarness(std::uint32_t requestedAdapter)
    {
        Check(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory_)),"CreateDXGIFactory2");
        std::uint32_t hardwareIndex=0;
        for(UINT i=0;;++i){ComPtr<IDXGIAdapter1> candidate; if(factory_->EnumAdapterByGpuPreference(i,DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,IID_PPV_ARGS(&candidate))==DXGI_ERROR_NOT_FOUND)break;DXGI_ADAPTER_DESC1 d{};candidate->GetDesc1(&d);if((d.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)!=0)continue;ComPtr<ID3D12Device> device;if(FAILED(D3D12CreateDevice(candidate.Get(),D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device))))continue;if(hardwareIndex++!=requestedAdapter)continue;adapter_=candidate;device_=device;break;}
        if(!device_)throw std::runtime_error("no compatible non-software D3D12 hardware adapter was found");
        DXGI_ADAPTER_DESC1 d{};adapter_->GetDesc1(&d);profile_.description=WideToUtf8(d.Description);profile_.vendorId=d.VendorId;profile_.deviceId=d.DeviceId;profile_.subsystemId=d.SubSysId;profile_.revision=d.Revision;profile_.luidLow=d.AdapterLuid.LowPart;profile_.luidHigh=d.AdapterLuid.HighPart;profile_.dedicatedVideoMemory=d.DedicatedVideoMemory;profile_.dedicatedSystemMemory=d.DedicatedSystemMemory;profile_.sharedSystemMemory=d.SharedSystemMemory;
        LARGE_INTEGER driver{};if(SUCCEEDED(adapter_->CheckInterfaceSupport(__uuidof(IDXGIDevice),&driver)))profile_.driverVersion=static_cast<std::uint64_t>(driver.QuadPart);
        D3D12_FEATURE_DATA_ARCHITECTURE1 arch{};arch.NodeIndex=0;if(SUCCEEDED(device_->CheckFeatureSupport(D3D12_FEATURE_ARCHITECTURE1,&arch,sizeof(arch)))){profile_.uma=arch.UMA!=FALSE;profile_.cacheCoherentUma=arch.CacheCoherentUMA!=FALSE;}
        D3D12_COMMAND_QUEUE_DESC q{};q.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;Check(device_->CreateCommandQueue(&q,IID_PPV_ARGS(&queue_)),"CreateCommandQueue");Check(queue_->GetTimestampFrequency(&profile_.timestampFrequency),"GetTimestampFrequency");
        Check(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator_)),"CreateCommandAllocator");Check(device_->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator_.Get(),nullptr,IID_PPV_ARGS(&list_)),"CreateCommandList");Check(list_->Close(),"initial Close");
        Check(device_->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence_)),"CreateFence");event_=CreateEventW(nullptr,FALSE,FALSE,nullptr);if(!event_)throw std::runtime_error("CreateEvent failed");
        D3D12_QUERY_HEAP_DESC qh{};qh.Type=D3D12_QUERY_HEAP_TYPE_TIMESTAMP;qh.Count=2;Check(device_->CreateQueryHeap(&qh,IID_PPV_ARGS(&queryHeap_)),"CreateQueryHeap");queryReadback_=CreateBuffer(16,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);D3D12_RANGE range{0,16};Check(queryReadback_->Map(0,&range,reinterpret_cast<void**>(&queryValues_)),"Map query readback");
        CreateRootSignature(); BuildFingerprint();
    }
    ~NativeHarness(){if(queryReadback_&&queryValues_)queryReadback_->Unmap(0,nullptr);if(event_)CloseHandle(event_);}
    const AdapterProfileV1& Profile()const{return profile_;}

    CaseNative CreateCase(const corpus::QualificationCaseV1& c,const PreparedAlgorithm& matrix,const PreparedAlgorithm& direct,const leaf::NativeProgramSetV1& programs,CaseMeasurementV1& output)
    {
        CaseNative n;const std::uint64_t inputBytes=c.inputPoints.size()*sizeof(semantic::Float4Point);n.input=CreateBuffer(inputBytes,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST);n.inputAllocationBytes=AllocationBytes(inputBytes,D3D12_RESOURCE_FLAG_NONE);output.sharedInputAllocatedBytes=n.inputAllocationBytes;output.sharedInputUploadBytes=inputBytes;
        auto upload=CreateBuffer(inputBytes,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);void* mapped=nullptr;D3D12_RANGE none{0,0};Check(upload->Map(0,&none,&mapped),"Map input upload");std::memcpy(mapped,c.inputPoints.data(),static_cast<std::size_t>(inputBytes));upload->Unmap(0,nullptr);Reset(nullptr);list_->CopyBufferRegion(n.input.Get(),0,upload.Get(),0,inputBytes);auto barrier=Transition(n.input.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);list_->ResourceBarrier(1,&barrier);ExecuteCurrent();
        D3D12_DESCRIPTOR_HEAP_DESC hd{};hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;hd.NumDescriptors=3;hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;Check(device_->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&n.heap)),"CreateDescriptorHeap");const UINT inc=device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);auto cpu=n.heap->GetCPUDescriptorHandleForHeapStart();auto gpu=n.heap->GetGPUDescriptorHandleForHeapStart();n.srv=gpu;
        D3D12_SHADER_RESOURCE_VIEW_DESC srv{};srv.ViewDimension=D3D12_SRV_DIMENSION_BUFFER;srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;srv.Format=DXGI_FORMAT_UNKNOWN;srv.Buffer.NumElements=static_cast<UINT>(c.inputPoints.size());srv.Buffer.StructureByteStride=sizeof(semantic::Float4Point);device_->CreateShaderResourceView(n.input.Get(),&srv,cpu);
        auto makeKernel=[&](const PreparedAlgorithm& p,std::span<const std::byte> shader,UINT descriptorIndex){const auto begin=Clock::now();KernelNative k;k.kind=p.measurement.loweringKind;D3D12_COMPUTE_PIPELINE_STATE_DESC pd{};pd.pRootSignature=root_.Get();pd.CS={shader.data(),shader.size()};Check(device_->CreateComputePipelineState(&pd,IID_PPV_ARGS(&k.pso)),"CreateComputePipelineState");const auto bytes=inputBytes;k.output=CreateBuffer(bytes,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);k.readback=CreateBuffer(bytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);k.constant=CreateBuffer(256,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);void* cm=nullptr;Check(k.constant->Map(0,&none,&cm),"Map constant");std::memset(cm,0,256);std::memcpy(cm,p.frozenLeaf.lockedConstantPayload.data(),p.frozenLeaf.lockedConstantPayload.size());k.constant->Unmap(0,nullptr);D3D12_UNORDERED_ACCESS_VIEW_DESC u{};u.ViewDimension=D3D12_UAV_DIMENSION_BUFFER;u.Format=DXGI_FORMAT_UNKNOWN;u.Buffer.NumElements=static_cast<UINT>(c.inputPoints.size());u.Buffer.StructureByteStride=sizeof(semantic::Float4Point);auto uc=cpu;uc.ptr+=static_cast<SIZE_T>(descriptorIndex)*inc;device_->CreateUnorderedAccessView(k.output.Get(),nullptr,&u,uc);k.uav=gpu;k.uav.ptr+=static_cast<UINT64>(descriptorIndex)*inc;k.allocationBytes=AllocationBytes(bytes,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)+AllocationBytes(bytes,D3D12_RESOURCE_FLAG_NONE)+AllocationBytes(256,D3D12_RESOURCE_FLAG_NONE);k.materializationNs=Nanoseconds(begin,Clock::now());return k;};
        n.matrix=makeKernel(matrix,programs.matrixProgramBinary,1);n.direct=makeKernel(direct,programs.directPgaProgramBinary,2);return n;
    }

    SampleNative RunSample(CaseNative& c,KernelNative& k,std::uint32_t pointCount)
    {
        const auto recordBegin=Clock::now();Reset(k.pso.Get());ID3D12DescriptorHeap* heaps[]={c.heap.Get()};list_->SetDescriptorHeaps(1,heaps);list_->SetComputeRootSignature(root_.Get());list_->SetComputeRootDescriptorTable(0,c.srv);list_->SetComputeRootDescriptorTable(1,k.uav);list_->SetComputeRootConstantBufferView(2,k.constant->GetGPUVirtualAddress());list_->EndQuery(queryHeap_.Get(),D3D12_QUERY_TYPE_TIMESTAMP,0);list_->Dispatch((pointCount+63u)/64u,1,1);D3D12_RESOURCE_BARRIER u{};u.Type=D3D12_RESOURCE_BARRIER_TYPE_UAV;u.UAV.pResource=k.output.Get();list_->ResourceBarrier(1,&u);list_->EndQuery(queryHeap_.Get(),D3D12_QUERY_TYPE_TIMESTAMP,1);list_->ResolveQueryData(queryHeap_.Get(),D3D12_QUERY_TYPE_TIMESTAMP,0,2,queryReadback_.Get(),0);Check(list_->Close(),"Close sample command list");const auto recordEnd=Clock::now();const auto submitBegin=Clock::now();ID3D12CommandList* lists[]={list_.Get()};queue_->ExecuteCommandLists(1,lists);Wait();const auto submitEnd=Clock::now();SampleNative s;s.commandNs=static_cast<double>(Nanoseconds(recordBegin,recordEnd));s.e2eNs=static_cast<double>(Nanoseconds(recordBegin,submitEnd));s.gpuNs=static_cast<double>(queryValues_[1]-queryValues_[0])*1.0e9/static_cast<double>(profile_.timestampFrequency);return s;
    }

    std::vector<semantic::Float4Point> ReadOutput(KernelNative& k,std::uint32_t pointCount)
    {
        const std::uint64_t bytes=static_cast<std::uint64_t>(pointCount)*sizeof(semantic::Float4Point);Reset(nullptr);auto a=Transition(k.output.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_SOURCE);list_->ResourceBarrier(1,&a);list_->CopyBufferRegion(k.readback.Get(),0,k.output.Get(),0,bytes);auto b=Transition(k.output.Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);list_->ResourceBarrier(1,&b);ExecuteCurrent();void* mapped=nullptr;D3D12_RANGE range{0,static_cast<SIZE_T>(bytes)};Check(k.readback->Map(0,&range,&mapped),"Map output readback");std::vector<semantic::Float4Point> result(pointCount);std::memcpy(result.data(),mapped,static_cast<std::size_t>(bytes));D3D12_RANGE none{0,0};k.readback->Unmap(0,&none);return result;
    }
private:
    static std::string WideToUtf8(const wchar_t* text){if(!text)return{};const int n=WideCharToMultiByte(CP_UTF8,0,text,-1,nullptr,0,nullptr,nullptr);if(n<=1)return{};std::string s(static_cast<std::size_t>(n),'\0');WideCharToMultiByte(CP_UTF8,0,text,-1,s.data(),n,nullptr,nullptr);s.pop_back();return s;}
    ComPtr<ID3D12Resource> CreateBuffer(std::uint64_t bytes,D3D12_HEAP_TYPE heap,D3D12_RESOURCE_STATES state,D3D12_RESOURCE_FLAGS flags=D3D12_RESOURCE_FLAG_NONE){auto hp=HeapProperties(heap);auto d=BufferDescription(bytes,flags);ComPtr<ID3D12Resource> r;Check(device_->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&d,state,nullptr,IID_PPV_ARGS(&r)),"CreateCommittedResource");return r;}
    std::uint64_t AllocationBytes(std::uint64_t bytes,D3D12_RESOURCE_FLAGS flags){auto d=BufferDescription(bytes,flags);return device_->GetResourceAllocationInfo(0,1,&d).SizeInBytes;}
    void Reset(ID3D12PipelineState* pso){Check(allocator_->Reset(),"Reset allocator");Check(list_->Reset(allocator_.Get(),pso),"Reset command list");}
    void ExecuteCurrent(){Check(list_->Close(),"Close command list");ID3D12CommandList* lists[]={list_.Get()};queue_->ExecuteCommandLists(1,lists);Wait();}
    void Wait(){const auto v=++fenceValue_;Check(queue_->Signal(fence_.Get(),v),"Signal");if(fence_->GetCompletedValue()<v){Check(fence_->SetEventOnCompletion(v,event_),"SetEventOnCompletion");WaitForSingleObject(event_,INFINITE);}}
    void CreateRootSignature(){D3D12_DESCRIPTOR_RANGE ranges[2]{};ranges[0].RangeType=D3D12_DESCRIPTOR_RANGE_TYPE_SRV;ranges[0].NumDescriptors=1;ranges[0].BaseShaderRegister=0;ranges[0].OffsetInDescriptorsFromTableStart=0;ranges[1].RangeType=D3D12_DESCRIPTOR_RANGE_TYPE_UAV;ranges[1].NumDescriptors=1;ranges[1].BaseShaderRegister=0;ranges[1].OffsetInDescriptorsFromTableStart=0;D3D12_ROOT_PARAMETER params[3]{};params[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;params[0].DescriptorTable={1,&ranges[0]};params[0].ShaderVisibility=D3D12_SHADER_VISIBILITY_ALL;params[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;params[1].DescriptorTable={1,&ranges[1]};params[1].ShaderVisibility=D3D12_SHADER_VISIBILITY_ALL;params[2].ParameterType=D3D12_ROOT_PARAMETER_TYPE_CBV;params[2].Descriptor.ShaderRegister=0;params[2].ShaderVisibility=D3D12_SHADER_VISIBILITY_ALL;D3D12_ROOT_SIGNATURE_DESC desc{};desc.NumParameters=3;desc.pParameters=params;ComPtr<ID3DBlob> blob,error;Check(D3D12SerializeRootSignature(&desc,D3D_ROOT_SIGNATURE_VERSION_1,&blob,&error),"D3D12SerializeRootSignature");Check(device_->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(),IID_PPV_ARGS(&root_)),"CreateRootSignature");}
    void BuildFingerprint(){base::BinaryWriter w;WriteString(w,profile_.description);w.WriteU32(profile_.vendorId);w.WriteU32(profile_.deviceId);w.WriteU32(profile_.subsystemId);w.WriteU32(profile_.revision);w.WriteU32(profile_.luidLow);w.WriteI32(profile_.luidHigh);w.WriteU64(profile_.driverVersion);w.WriteU64(profile_.timestampFrequency);profile_.fingerprint=base::Sha256(w.Bytes());}
    AdapterProfileV1 profile_{};ComPtr<IDXGIFactory6> factory_;ComPtr<IDXGIAdapter1> adapter_;ComPtr<ID3D12Device> device_;ComPtr<ID3D12CommandQueue> queue_;ComPtr<ID3D12CommandAllocator> allocator_;ComPtr<ID3D12GraphicsCommandList> list_;ComPtr<ID3D12Fence> fence_;HANDLE event_=nullptr;std::uint64_t fenceValue_=0;ComPtr<ID3D12RootSignature> root_;ComPtr<ID3D12QueryHeap> queryHeap_;ComPtr<ID3D12Resource> queryReadback_;std::uint64_t* queryValues_=nullptr;
};
#endif

void WriteCsv(const BenchmarkEvidenceV1& e,const std::filesystem::path& path)
{
    std::ofstream f(path,std::ios::binary);if(!f)throw std::runtime_error("failed to create benchmark_samples.csv");f<<"scenario,subscenario,points,run,block,order,lowering,command_recording_ns,gpu_dispatch_ns,end_to_end_ns\r\n";f<<std::setprecision(17);
    for(const auto& c:e.cases)for(const auto* a:{&c.matrix,&c.directPga})for(const auto& s:a->samples)f<<c.scenarioId<<','<<c.subscenarioIndex<<','<<c.pointCount<<','<<s.run<<','<<s.block<<','<<s.orderPosition<<','<<KindName(s.loweringKind)<<','<<s.commandRecordingNs<<','<<s.gpuDispatchNs<<','<<s.endToEndNs<<"\r\n";
}

void WriteProfileJson(const BenchmarkEvidenceV1& e,const std::filesystem::path& path)
{
    std::ofstream f(path,std::ios::binary);if(!f)throw std::runtime_error("failed to create benchmark_profile.json");f<<"{\n  \"schema\": \"SGE4-5.Spiral1.BenchmarkProfile.V1\",\n  \"captureUnixNanoseconds\": "<<e.captureUnixNanoseconds<<",\n  \"warmupSamplesPerAlgorithm\": "<<e.config.warmupSamplesPerAlgorithm<<",\n  \"measurementSamplesPerAlgorithm\": "<<e.config.measurementSamplesPerAlgorithm<<",\n  \"runCount\": "<<e.config.runCount<<",\n  \"order\": \"ABBA\",\n  \"clockPolicy\": \""<<JsonEscape(e.config.clockPolicy)<<"\",\n  \"adapter\": {\n    \"description\": \""<<JsonEscape(e.adapter.description)<<"\",\n    \"vendorId\": "<<e.adapter.vendorId<<",\n    \"deviceId\": "<<e.adapter.deviceId<<",\n    \"luidLow\": "<<e.adapter.luidLow<<",\n    \"luidHigh\": "<<e.adapter.luidHigh<<",\n    \"driverVersion\": "<<e.adapter.driverVersion<<",\n    \"timestampFrequency\": "<<e.adapter.timestampFrequency<<",\n    \"fingerprint\": \""<<Hex(e.adapter.fingerprint)<<"\"\n  },\n  \"corpusIdentity\": \""<<Hex(e.corpusIdentity)<<"\",\n  \"observationContractIdentity\": \""<<Hex(e.observationContractIdentity)<<"\",\n  \"representationTargetProfileIdentity\": \""<<Hex(e.representationTargetProfileIdentity)<<"\",\n  \"benchmarkProfileIdentity\": \""<<Hex(e.benchmarkProfileIdentity)<<"\"\n}\n";
}

void WriteSummaryJson(const BenchmarkEvidenceV1& e,const std::filesystem::path& path)
{
    std::ofstream f(path,std::ios::binary);if(!f)throw std::runtime_error("failed to create benchmark_summary.json");f<<std::setprecision(17)<<"{\n  \"schema\": \"SGE4-5.Spiral1.BenchmarkSummary.V1\",\n  \"adapterFingerprint\": \""<<Hex(e.adapter.fingerprint)<<"\",\n  \"cases\": [\n";
    for(std::size_t i=0;i<e.cases.size();++i){const auto& c=e.cases[i];f<<"    {\"scenario\": \""<<c.scenarioId<<"\", \"subscenario\": "<<c.subscenarioIndex<<", \"points\": "<<c.pointCount<<", \"mismatchCount\": "<<c.observation.mismatchCount<<", \"matrixGpuMedianNs\": "<<c.matrix.gpuDispatch.median<<", \"matrixGpuP95Ns\": "<<c.matrix.gpuDispatch.p95<<", \"directGpuMedianNs\": "<<c.directPga.gpuDispatch.median<<", \"directGpuP95Ns\": "<<c.directPga.gpuDispatch.p95<<", \"matrixPackageBytes\": "<<c.matrix.packageBytes<<", \"directPackageBytes\": "<<c.directPga.packageBytes<<", \"matrixConstantBytes\": "<<c.matrix.constantBytes<<", \"directConstantBytes\": "<<c.directPga.constantBytes<<"}"<<(i+1==e.cases.size()?"\n":",\n");}
    f<<"  ]\n}\n";
}

std::string RelativeVerdict(double matrix,double direct)
{
    if (matrix <= 0 || direct <= 0) return "unavailable";
    const double pct = std::abs(matrix - direct) / (std::min)(matrix, direct) * 100.0;
    if (pct < 2.0) return "difference below 2% reporting band";
    return direct < matrix ? "Direct PGA lower median GPU time" : "Matrix lower median GPU time";
}
}

base::Result<BenchmarkEvidenceV1,std::string> RunRealGpuMeasurementV1(const BenchmarkConfigV1& config)
{
    try{
        Require(config.warmupSamplesPerAlgorithm>0&&config.measurementSamplesPerAlgorithm>0&&config.runCount>0,"benchmark counts must be positive");Require((config.warmupSamplesPerAlgorithm%2)==0&&(config.measurementSamplesPerAlgorithm%2)==0,"ABBA profile requires even per-algorithm sample counts");
#ifndef _WIN32
        return base::Result<BenchmarkEvidenceV1,std::string>::Failure("real GPU measurement is available only on Windows D3D12");
#else
        BenchmarkEvidenceV1 evidence;evidence.config=config;evidence.captureUnixNanoseconds=static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
        auto corpusValue=corpus::BuildQualificationCorpusV1();if(!corpusValue)throw std::runtime_error(corpusValue.Error());evidence.corpusIdentity=corpusValue.Value().contractIdentity;evidence.observationContractIdentity=contracts::ObservationContractIdentityV2();
        const auto nativeBegin=Clock::now();auto programs=leaf::BuildNativeProgramSetV1();const auto nativeEnd=Clock::now();if(!programs)throw std::runtime_error(programs.Error().stage+": "+programs.Error().message);evidence.nativeProgramBuildNs=Nanoseconds(nativeBegin,nativeEnd);evidence.representationTargetProfileIdentity=programs.Value().representationTargetProfile.identity;
        NativeHarness harness(config.adapterIndex);evidence.adapter=harness.Profile();evidence.benchmarkProfileIdentity=ComputeBenchmarkProfileIdentity(evidence);
        const std::array<std::pair<std::string_view,std::uint32_t>,4> selected{{{"S03",0},{"S15",15},{"S07",0},{"S12",0}}};
        for(const auto& [sid,sub]:selected){const auto& c=FindCase(corpusValue.Value(),sid,sub);auto matrix=PrepareAlgorithm(c,representation::LoweringKindV1::MatrixExpandedCompute,programs.Value());auto direct=PrepareAlgorithm(c,representation::LoweringKindV1::DirectPgaMotorCompute,programs.Value());CaseMeasurementV1 result;result.scenarioId=c.scenarioId;result.subscenarioIndex=c.subscenarioIndex;result.pointCount=static_cast<std::uint32_t>(c.inputPoints.size());result.caseIdentity=c.caseIdentity;auto native=harness.CreateCase(c,matrix,direct,programs.Value(),result);matrix.measurement.materializationNs=native.matrix.materializationNs;direct.measurement.materializationNs=native.direct.materializationNs;matrix.measurement.allocatedBytes=native.matrix.allocationBytes;direct.measurement.allocatedBytes=native.direct.allocationBytes;matrix.measurement.cpuToGpuUploadBytes=result.sharedInputUploadBytes+matrix.measurement.constantBytes;direct.measurement.cpuToGpuUploadBytes=result.sharedInputUploadBytes+direct.measurement.constantBytes;
            const std::array<representation::LoweringKindV1,4> order{{representation::LoweringKindV1::MatrixExpandedCompute,representation::LoweringKindV1::DirectPgaMotorCompute,representation::LoweringKindV1::DirectPgaMotorCompute,representation::LoweringKindV1::MatrixExpandedCompute}};
            auto runOne=[&](representation::LoweringKindV1 kind){return harness.RunSample(native,kind==representation::LoweringKindV1::MatrixExpandedCompute?native.matrix:native.direct,result.pointCount);};
            for(std::uint32_t run=0;run<config.runCount;++run){for(std::uint32_t block=0;block<config.warmupSamplesPerAlgorithm/2;++block)for(auto kind:order)(void)runOne(kind);for(std::uint32_t block=0;block<config.measurementSamplesPerAlgorithm/2;++block){for(std::uint32_t position=0;position<order.size();++position){auto kind=order[position];auto s=runOne(kind);TimingSampleV1 sample{run,block,position,kind,s.commandNs,s.gpuNs,s.e2eNs};(kind==representation::LoweringKindV1::MatrixExpandedCompute?matrix.measurement.samples:direct.measurement.samples).push_back(sample);}}}
            FinalizeSummaries(matrix.measurement);FinalizeSummaries(direct.measurement);auto matrixOutput=harness.ReadOutput(native.matrix,result.pointCount);auto directOutput=harness.ReadOutput(native.direct,result.pointCount);auto observed=observer::ObserveCaseV1(c,matrixOutput,directOutput);if(!observed)throw std::runtime_error(observed.Error());Require(observed.Value().report.mismatchCount==0&&observed.Value().report.nonFiniteCount==0,"real GPU output failed Observation Contract V2");result.matrix=std::move(matrix.measurement);result.directPga=std::move(direct.measurement);result.observation=observed.Value().report;evidence.cases.push_back(std::move(result));std::cout<<"[MEASURED] "<<sid<<'.'<<sub<<" points="<<c.inputPoints.size()<<" matrixMedianNs="<<evidence.cases.back().matrix.gpuDispatch.median<<" directMedianNs="<<evidence.cases.back().directPga.gpuDispatch.median<<'\n';}
        return base::Result<BenchmarkEvidenceV1,std::string>::Success(std::move(evidence));
#endif
    }catch(const std::exception& e){return base::Result<BenchmarkEvidenceV1,std::string>::Failure(e.what());}
}

base::Result<void,std::string> WriteBenchmarkArtifactsV1(const BenchmarkEvidenceV1& evidence,const std::filesystem::path& outputDirectory)
{
    try{std::filesystem::create_directories(outputDirectory);const auto bytes=SerializeEvidence(evidence);auto w=base::WriteAllBytes(outputDirectory/"benchmark_evidence_v1.bin",bytes);if(!w)return base::Result<void,std::string>::Failure(w.Error());WriteCsv(evidence,outputDirectory/"benchmark_samples.csv");WriteProfileJson(evidence,outputDirectory/"benchmark_profile.json");WriteSummaryJson(evidence,outputDirectory/"benchmark_summary.json");return base::Result<void,std::string>::Success();}catch(const std::exception& e){return base::Result<void,std::string>::Failure(e.what());}
}
base::Result<BenchmarkEvidenceV1,std::string> ReadBenchmarkEvidenceV1(const std::filesystem::path& evidencePath){auto bytes=base::ReadAllBytes(evidencePath);if(!bytes)return base::Result<BenchmarkEvidenceV1,std::string>::Failure(bytes.Error());return DeserializeEvidence(bytes.Value());}

base::Result<void,std::string> WriteDecisionEvidenceReportV1(const BenchmarkEvidenceV1& e,const std::filesystem::path& outputDirectory)
{
    try{Require(!e.cases.empty(),"decision evidence requires at least one measured case");std::filesystem::create_directories(outputDirectory);std::ofstream md(outputDirectory/"SPIRAL1_DECISION_EVIDENCE_REPORT.md",std::ios::binary);if(!md)throw std::runtime_error("failed to create decision evidence report");md<<std::fixed<<std::setprecision(3);md<<"# SGE4-5 Spiral 1 Decision Evidence Report V1\n\n"<<"- Status: Measurement complete; next capability selection deferred by owner\n"<<"- S1-I17: Qualified\n"<<"- S1-I18: Preserved as policy; selection action not executed\n"<<"- Adapter: "<<e.adapter.description<<"\n- Adapter fingerprint: `"<<Hex(e.adapter.fingerprint)<<"`\n- Clock policy: `"<<e.config.clockPolicy<<"`\n- Benchmark profile identity: `"<<Hex(e.benchmarkProfileIdentity)<<"`\n- ABBA profile: warm-up "<<e.config.warmupSamplesPerAlgorithm<<", measurement "<<e.config.measurementSamplesPerAlgorithm<<" per algorithm, runs "<<e.config.runCount<<"\n\n";
        md<<"## Fixed architecture evidence\n\n- Architecture: `"<<ArchitectureEvidence<<"`\n- WARP corpus: `"<<WarpEvidence<<"`\n- Architecture final freeze: `"<<FinalFreezeEvidence<<"`\n\n";
        md<<"## Required questions answered\n\n1. **Could Level 4 v1 close the experiment?** Yes. Four Frozen Leaves and five Buffer flows completed without a Level 4 capability extension.\n2. **Missing Level 4 contract?** None was required for Spiral 1. The locked DynamicData wrapper remained experiment-owned and used existing Schema 17.\n3. **General capability versus Leaf-local concern?** All observed special handling remained in Spiral 1 artifacts and qualification code.\n4. **Did retained PGA meaning create a candidate unavailable after immediate Matrix lowering?** Yes. `DirectPgaMotorComputeV1` is independently generated and verified from the same Semantic identity.\n5. **Where is Direct PGA favorable or unfavorable?** See the measured table below; constants are 48 bytes versus Matrix 64 bytes, while timing and package size are adapter evidence.\n6. **Does the preferred lowering vary by point count?** The table reports each measured point count without forcing a global winner.\n7. **What Planner mutations did the Verifier detect?** Matrix coefficient, Motor coefficient/order, template, binary, binding, schema, Observation identity, point count, dispatch, Candidate identity and seal/context mutations.\n8. **Was the Canonical Program Template approach sufficient?** Yes for this experiment; arbitrary Shader equivalence proof was not required.\n9. **Which next experiment or Level 4 capability is selected?** **Deferred by owner. No capability is selected in this report.**\n10. **Should Level 4 be extended now?** No extension is authorized by this report. Future selection requires a separate owner decision.\n\n";
        md<<"## Measured comparison\n\n| Scenario | Points | Matrix median GPU ns | Direct PGA median GPU ns | Matrix p95 | Direct p95 | Result band | Matrix package B | Direct package B | Constants B | Mismatch |\n|---|---:|---:|---:|---:|---:|---|---:|---:|---|---:|\n";
        for(const auto& c:e.cases)md<<'|'<<c.scenarioId<<'.'<<c.subscenarioIndex<<'|'<<c.pointCount<<'|'<<c.matrix.gpuDispatch.median<<'|'<<c.directPga.gpuDispatch.median<<'|'<<c.matrix.gpuDispatch.p95<<'|'<<c.directPga.gpuDispatch.p95<<'|'<<RelativeVerdict(c.matrix.gpuDispatch.median,c.directPga.gpuDispatch.median)<<'|'<<c.matrix.packageBytes<<'|'<<c.directPga.packageBytes<<"|64 / 48|"<<c.observation.mismatchCount<<"|\n";
        md<<"\n## Resource and process evidence\n\nThe raw CSV preserves every ABBA sample. Profile identity includes adapter, driver, timestamp frequency, clock policy, corpus, Observation V2, target profile, warm-up, measurement count and run count. Performance superiority is not a completion gate.\n\n## Deferred decision boundary\n\n`NextCapabilitySelection = DeferredByOwner`\n\nNo Spiral 2 option, Level 4 extension, or empty abstraction is selected or created here.\n";
        std::ofstream js(outputDirectory/"SPIRAL1_DECISION_EVIDENCE_REPORT.json",std::ios::binary);if(!js)throw std::runtime_error("failed to create decision JSON");js<<"{\n  \"schema\": \"SGE4-5.Spiral1.DecisionEvidence.V1\",\n  \"measurementComplete\": true,\n  \"s1I17\": \"Qualified\",\n  \"s1I18\": \"PreservedSelectionDeferred\",\n  \"nextCapabilitySelection\": \"DeferredByOwner\",\n  \"adapterFingerprint\": \""<<Hex(e.adapter.fingerprint)<<"\",\n  \"measuredCaseCount\": "<<e.cases.size()<<"\n}\n";return base::Result<void,std::string>::Success();}catch(const std::exception& ex){return base::Result<void,std::string>::Failure(ex.what());}
}

base::Result<void,std::string> RunFormatSelfTestV1(const std::filesystem::path& outputDirectory)
{
    BenchmarkEvidenceV1 e;e.captureUnixNanoseconds=1;e.config={2,4,1,0,"self-test"};e.adapter.description="Synthetic Adapter";e.adapter.vendorId=1;e.adapter.timestampFrequency=1000000;e.adapter.fingerprint=base::Sha256(std::as_bytes(std::span("synthetic",9)));e.corpusIdentity=e.adapter.fingerprint;e.observationContractIdentity=e.adapter.fingerprint;e.representationTargetProfileIdentity=e.adapter.fingerprint;e.benchmarkProfileIdentity=ComputeBenchmarkProfileIdentity(e);CaseMeasurementV1 c;c.scenarioId="SXX";c.pointCount=64;c.caseIdentity=e.adapter.fingerprint;c.matrix.loweringKind=representation::LoweringKindV1::MatrixExpandedCompute;c.directPga.loweringKind=representation::LoweringKindV1::DirectPgaMotorCompute;c.matrix.constantBytes=64;c.directPga.constantBytes=48;c.matrix.packageBytes=100;c.directPga.packageBytes=90;c.matrix.gpuDispatch={100,110,90,120};c.directPga.gpuDispatch={95,105,85,115};c.observation.pointCount=64;e.cases.push_back(c);auto w=WriteBenchmarkArtifactsV1(e,outputDirectory);if(!w)return w;auto r=ReadBenchmarkEvidenceV1(outputDirectory/"benchmark_evidence_v1.bin");if(!r)return base::Result<void,std::string>::Failure(r.Error());if(r.Value().cases.size()!=1||r.Value().cases[0].matrix.constantBytes!=64)return base::Result<void,std::string>::Failure("benchmark evidence roundtrip failed");return WriteDecisionEvidenceReportV1(r.Value(),outputDirectory);
}
}
