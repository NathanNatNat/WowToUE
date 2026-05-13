/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>, Marlamin <marlamin@marlamin.com>
	License: MIT
 */
#pragma once

#include <cstdint>

namespace db {

/**
 * Field types for DB definitions.
 * JS uses Symbol() for unique identity; C++ uses a strongly-typed enum.
 */
enum class FieldType : uint32_t {
	String,
	Int8,
	UInt8,
	Int16,
	UInt16,
	Int32,
	UInt32,
	Int64,
	UInt64,
	Float,
	Relation,
	NonInlineID
};

} // namespace db
