#pragma once

#include "../../canonical/identity/CanonicalVocabulary.h"

namespace sge4::composition
{
struct CompositionContractIdentityTag;
struct CompositionPlanIdentityTag;
struct CompositionSealIdentityTag;
struct FrozenCompositionIdentityTag;
struct ScheduleIdentityTag;
struct RecoverySetIdentityTag;

using CompositionContractIdentity = ::sge4::canonical::CanonicalIdentityV1<CompositionContractIdentityTag>;
using CompositionPlanIdentity = ::sge4::canonical::CanonicalIdentityV1<CompositionPlanIdentityTag>;
using CompositionSealIdentity = ::sge4::canonical::CanonicalIdentityV1<CompositionSealIdentityTag>;
using FrozenCompositionIdentity = ::sge4::canonical::CanonicalIdentityV1<FrozenCompositionIdentityTag>;
using ScheduleIdentity = ::sge4::canonical::CanonicalIdentityV1<ScheduleIdentityTag>;
using RecoverySetIdentity = ::sge4::canonical::CanonicalIdentityV1<RecoverySetIdentityTag>;
}
