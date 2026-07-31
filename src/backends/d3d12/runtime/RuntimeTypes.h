#pragma once

#include "../../../composition/model/CompositionContract.h"
#include "../../../composition/model/plan/CompositionPlan.h"
#include "../../../composition/artifact/VerifiedCompositionArtifact.h"

namespace sge4::d3d12::runtime_detail
{
namespace model = ::sge4::composition;
namespace planning = ::sge4::composition::planning;
namespace verification = ::sge4::composition::verification;
namespace artifact = ::sge4::composition::artifact;

using LeafPackageId = model::LeafPackageId;
using CompositionEndpointId = model::CompositionEndpointId;
using ResourceFlowId = model::ResourceFlowId;
using StableKey = model::StableKey;
using ResourceBoundary = model::ResourceBoundary;
using EndpointAccess = model::EndpointAccess;
using CompositionEndpointContract = model::CompositionEndpointContract;
using PackageCompositionContract = model::PackageCompositionContract;
}
