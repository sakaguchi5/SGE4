#include "../05_TargetContract/TargetModel.h"
#include "../11_D3D12PackageLowering/D3D12PackageLowering.h"
#include "../09_FrozenPackageCore/PackageReader.h"
#include "../21_ClassicalFrontend/ClassicalTriangle.h"
#include "../22_SdfFrontend/SdfFrontend.h"
#include "../23_PgaFrontend/PgaFrontend.h"
#include "../20_ExperimentDomain/ExperimentDomain.h"
#include "../12_SGE4_5Compiler/SGE4_5Compiler.h"

#include <cmath>
#include <iostream>

namespace
{
bool SamePackage(
    const sge4_5::compiler::d3d12::CompileOutput& left,
    const sge4_5::compiler::d3d12::CompileOutput& right)
{
    return left.packageBytes == right.packageBytes &&
           left.executionDigestHex == right.executionDigestHex;
}
}

int main()
{
    const auto classicalGeometry = sge4_5::classical::BuildTriangleGeometry();
    auto sdfGeometry = sge4_5::sdf::BuildTriangleGeometry();
    auto pgaGeometry = sge4_5::pga::BuildTriangleGeometry();
    if (!sdfGeometry || !pgaGeometry)
    {
        std::cerr << "Mathematical geometry extraction failed: "
                  << (!sdfGeometry ? sdfGeometry.Error() : pgaGeometry.Error()) << '\n';
        return 1;
    }

    if (!sge4_5::experiment::GeometryBitwiseEqual(classicalGeometry, sdfGeometry.Value()) ||
        !sge4_5::experiment::GeometryBitwiseEqual(classicalGeometry, pgaGeometry.Value()))
    {
        std::cerr << "Classical, SDF, and PGA frontends did not produce bit-identical geometry\n";
        return 2;
    }

    const auto sdfDescription = sge4_5::sdf::BuildTriangleDescription();
    const float inside = sge4_5::sdf::EvaluateSignedDistance(sdfDescription.nearField, {0.0f, 0.0f});
    const float boundary = sge4_5::sdf::EvaluateSignedDistance(sdfDescription.nearField, {0.0f, 0.5f});
    const float outside = sge4_5::sdf::EvaluateSignedDistance(sdfDescription.nearField, {0.75f, 0.5f});
    if (!(inside < 0.0f) || std::abs(boundary) > 0.000001f || !(outside > 0.0f))
    {
        std::cerr << "SDF sign contract failed: inside=" << inside
                  << ", boundary=" << boundary << ", outside=" << outside << '\n';
        return 3;
    }

    const auto pgaDescription = sge4_5::pga::BuildTriangleDescription();
    const auto& nearLines = pgaDescription.nearLayer.boundaryLines;
    auto apex = sge4_5::pga::Meet(nearLines[2], nearLines[1]);
    auto rightBase = sge4_5::pga::Meet(nearLines[1], nearLines[0]);
    auto leftBase = sge4_5::pga::Meet(nearLines[0], nearLines[2]);
    if (!apex || !rightBase || !leftBase)
    {
        std::cerr << "PGA finite meet failed\n";
        return 4;
    }
    if (std::abs(sge4_5::pga::Incidence(nearLines[1], apex.Value())) > 0.000001f ||
        !sge4_5::pga::ProjectivelyEquivalent(sge4_5::pga::Join(apex.Value(), rightBase.Value()), nearLines[1]) ||
        !sge4_5::pga::ProjectivelyEquivalent(sge4_5::pga::Join(rightBase.Value(), leftBase.Value()), nearLines[0]))
    {
        std::cerr << "PGA join/meet/incidence contract failed\n";
        return 5;
    }
    if (sge4_5::pga::Meet({0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, -1.0f}))
    {
        std::cerr << "PGA finite lowering accepted an ideal intersection of parallel lines\n";
        return 6;
    }

    auto classicalGraph = sge4_5::classical::BuildTriangleGraph();
    auto sdfGraph = sge4_5::sdf::BuildTriangleGraph();
    auto pgaGraph = sge4_5::pga::BuildTriangleGraph();
    if (!classicalGraph || !sdfGraph || !pgaGraph)
    {
        std::cerr << "Frontend lowering failed: "
                  << (!classicalGraph ? classicalGraph.Error() :
                      !sdfGraph ? sdfGraph.Error() : pgaGraph.Error()) << '\n';
        return 7;
    }

    sge4_5::target::D3D12TargetProfile profile;
    auto classicalFirst = sge4_5::compiler::CompileCanonical(classicalGraph.Value(), profile);
    auto classicalSecond = sge4_5::compiler::CompileCanonical(classicalGraph.Value(), profile);
    auto sdfFirst = sge4_5::compiler::CompileCanonical(sdfGraph.Value(), profile);
    auto sdfSecond = sge4_5::compiler::CompileCanonical(sdfGraph.Value(), profile);
    auto pgaFirst = sge4_5::compiler::CompileCanonical(pgaGraph.Value(), profile);
    auto pgaSecond = sge4_5::compiler::CompileCanonical(pgaGraph.Value(), profile);
    if (!classicalFirst || !classicalSecond || !sdfFirst || !sdfSecond || !pgaFirst || !pgaSecond)
    {
        const auto* error = !classicalFirst ? &classicalFirst.Error() :
                            !classicalSecond ? &classicalSecond.Error() :
                            !sdfFirst ? &sdfFirst.Error() :
                            !sdfSecond ? &sdfSecond.Error() :
                            !pgaFirst ? &pgaFirst.Error() : &pgaSecond.Error();
        std::cerr << "Compile failed at " << error->stage << ": " << error->message << '\n';
        return 8;
    }

    if (!SamePackage(classicalFirst.Value(), classicalSecond.Value()) ||
        !SamePackage(sdfFirst.Value(), sdfSecond.Value()) ||
        !SamePackage(pgaFirst.Value(), pgaSecond.Value()))
    {
        std::cerr << "A frontend did not produce deterministic Package bytes\n";
        return 9;
    }

    if (!SamePackage(classicalFirst.Value(), sdfFirst.Value()) ||
        !SamePackage(classicalFirst.Value(), pgaFirst.Value()))
    {
        std::cerr << "Classical, SDF, and PGA lowerings did not converge to one Frozen Package\n";
        return 10;
    }

    auto decoded = sge4_5::package::PackageReader::Read(classicalFirst.Value().packageBytes);
    if (!decoded)
    {
        std::cerr << "Common experiment Package validation failed: " << decoded.Error().message << '\n';
        return 11;
    }

    std::cout << "PGA join, meet, incidence, finite normalization, and ideal-point rejection passed.\n";
    std::cout << "Classical explicit-mesh, SDF half-space, and PGA line-algebra frontends converged to bit-identical geometry, Semantic execution, and Frozen Package bytes.\n";
    std::cout << "Experiment equivalence test passed.\n";
    return 0;
}
