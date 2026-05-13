/*!
wow.export (https://github.com/Kruithne/wow.export)
Authors: Kruithne <kruithne@gmail.com>, Marlamin <marlamin@marlamin.com>
License: MIT
*/

#include "DBComponentTextureFileData.h"

#include "../../log.h"
#include "../../casc/db2.h"
#include "../WDCReader.h"

#include <format>
#include <unordered_map>
#include <mutex>
#include <condition_variable>

namespace db::caches::DBComponentTextureFileData {

static uint32_t fieldToUint32(const db::FieldValue& val) {
if (auto* p = std::get_if<int64_t>(&val))
return static_cast<uint32_t>(*p);
if (auto* p = std::get_if<uint64_t>(&val))
return static_cast<uint32_t>(*p);
if (auto* p = std::get_if<float>(&val))
return static_cast<uint32_t>(*p);
return 0;
}

static constexpr uint32_t GENDER_ANY = 2;

static std::unordered_map<uint32_t, ComponentTextureInfo> file_data_to_info;

static bool is_initialized = false;
static bool is_initializing = false;
static std::mutex init_mutex;
static std::condition_variable init_cv;

void initialize() {
{
std::unique_lock lock(init_mutex);
if (is_initialized)
return;
if (is_initializing) {
init_cv.wait(lock, [] { return is_initialized || !is_initializing; });
return;
}
is_initializing = true;
}

try {
logging::write("Loading ComponentTextureFileData...");

auto allRows = casc::db2::preloadTable("ComponentTextureFileData").getAllRows();
for (const auto& [id, row] : allRows) {
ComponentTextureInfo info;
info.raceID = fieldToUint32(row.at("RaceID"));
info.genderIndex = fieldToUint32(row.at("GenderIndex"));
info.classID = fieldToUint32(row.at("ClassID"));
file_data_to_info.emplace(id, info);
}

logging::write(std::format("Loaded ComponentTextureFileData for {} textures", file_data_to_info.size()));
{
std::scoped_lock lock(init_mutex);
is_initialized = true;
is_initializing = false;
}
init_cv.notify_all();
} catch (...) {
{
std::scoped_lock lock(init_mutex);
is_initializing = false;
}
init_cv.notify_all();
throw;
}
}

std::future<void> initializeAsync() {
return std::async(std::launch::async, [] { initialize(); });
}

std::optional<uint32_t> getTextureForRaceGender(const std::vector<uint32_t>& file_data_ids, std::optional<uint32_t> race_id, std::optional<uint32_t> gender_index, uint32_t fallback_race_id) {
if (file_data_ids.empty())
return std::nullopt;

// if only one option, return it
if (file_data_ids.size() == 1)
return file_data_ids[0];

const uint32_t race = race_id.value_or(static_cast<uint32_t>(-1));
const uint32_t gender = gender_index.value_or(static_cast<uint32_t>(-1));

// try exact race + gender match
for (const auto fdid : file_data_ids) {
auto it = file_data_to_info.find(fdid);
if (it != file_data_to_info.end() && it->second.raceID == race && it->second.genderIndex == gender)
return fdid;
}

// try race + any gender
for (const auto fdid : file_data_ids) {
auto it = file_data_to_info.find(fdid);
if (it != file_data_to_info.end() && it->second.raceID == race && it->second.genderIndex == GENDER_ANY)
return fdid;
}

// try fallback race if provided
if (fallback_race_id > 0) {
for (const auto fdid : file_data_ids) {
auto it = file_data_to_info.find(fdid);
if (it != file_data_to_info.end() && it->second.raceID == fallback_race_id && (it->second.genderIndex == gender || it->second.genderIndex == GENDER_ANY))
return fdid;
}
}

// try race=0 (any race) with specific gender
for (const auto fdid : file_data_ids) {
auto it = file_data_to_info.find(fdid);
if (it != file_data_to_info.end() && it->second.raceID == 0 && it->second.genderIndex == gender)
return fdid;
}

// try race=0 (any race)
for (const auto fdid : file_data_ids) {
auto it = file_data_to_info.find(fdid);
if (it != file_data_to_info.end() && it->second.raceID == 0)
return fdid;
}

// fallback to first
return file_data_ids[0];
}

bool hasEntry(uint32_t file_data_id) {
return file_data_to_info.contains(file_data_id);
}

const ComponentTextureInfo* getInfo(uint32_t file_data_id) {
auto it = file_data_to_info.find(file_data_id);
if (it != file_data_to_info.end())
return &it->second;
return nullptr;
}

} // namespace db::caches::DBComponentTextureFileData
