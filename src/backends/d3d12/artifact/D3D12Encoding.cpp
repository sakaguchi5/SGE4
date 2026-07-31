#include "./D3D12Encoding.h"

#include "../../../canonical/base/BinaryIO.h"
#include "../../../canonical/base/CheckedMath.h"
#include "../../../canonical/base/Sha256.h"
#include "../../../canonical/base/SchemaValidation.h"
#include "../../../leaf/artifact/package/PackageWriter.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>

namespace sge4::package::d3d12_v13
{
namespace
{
#include "./detail/EncodingSchemaAndTables.inl"
#include "./detail/EncodingRecordDecoders.inl"
#include "./detail/EncodingValidation.inl"
}

#include "./detail/EncodingPackageCodec.inl"
#include "./detail/EncodingOperationPayloads.inl"
}
