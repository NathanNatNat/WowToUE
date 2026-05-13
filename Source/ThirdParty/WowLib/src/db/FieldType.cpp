/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>, Marlamin <marlamin@marlamin.com>
	License: MIT
 */
#include "FieldType.h"

namespace db {

[[maybe_unused]] constexpr FieldType String = FieldType::String;
[[maybe_unused]] constexpr FieldType Int8 = FieldType::Int8;
[[maybe_unused]] constexpr FieldType UInt8 = FieldType::UInt8;
[[maybe_unused]] constexpr FieldType Int16 = FieldType::Int16;
[[maybe_unused]] constexpr FieldType UInt16 = FieldType::UInt16;
[[maybe_unused]] constexpr FieldType Int32 = FieldType::Int32;
[[maybe_unused]] constexpr FieldType UInt32 = FieldType::UInt32;
[[maybe_unused]] constexpr FieldType Int64 = FieldType::Int64;
[[maybe_unused]] constexpr FieldType UInt64 = FieldType::UInt64;
[[maybe_unused]] constexpr FieldType Float = FieldType::Float;
[[maybe_unused]] constexpr FieldType Relation = FieldType::Relation;
[[maybe_unused]] constexpr FieldType NonInlineID = FieldType::NonInlineID;

} // namespace db
