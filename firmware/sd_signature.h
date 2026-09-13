#pragma once
#include <array>
#include <cstdint>
#include <cstring>

namespace lcars {
inline uint32_t sd_le32(const uint8_t *p) {
  return uint32_t(p[0]) | (uint32_t(p[1]) << 8) |
         (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}
inline const char *sd_volume_signature(const uint8_t *s) {
  if (s[510] != 0x55 || s[511] != 0xaa) return nullptr;
  if (std::memcmp(s + 3, "EXFAT   ", 8) == 0) return "exFAT signature";
  if (std::memcmp(s + 82, "FAT32   ", 8) == 0) return "FAT32 signature";
  if (std::memcmp(s + 54, "FAT16   ", 8) == 0) return "FAT16 signature";
  return nullptr;
}
// Inspect metadata only, without mounting a filesystem or modifying any sector.
// A signature identifies the format; it does not prove filesystem integrity.
template<typename ReadSector>
const char *sd_inspect(uint64_t sector_count, ReadSector read) {
  std::array<uint8_t, 512> sector{};
  if (!sector_count || !read(0, sector.data())) return "sector read failed";
  if (const char *format = sd_volume_signature(sector.data())) return format;
  if (sector[510] != 0x55 || sector[511] != 0xaa) return "readable; format unidentified";
  const auto mbr = sector;
  for (unsigned i = 0; i < 4; ++i) {
    const auto *entry = mbr.data() + 446 + i * 16;
    if (!entry[4]) continue;
    if (entry[4] == 0xee) return "readable; GPT (filesystem not inspected)";
    const uint64_t start = sd_le32(entry + 8), count = sd_le32(entry + 12);
    if (!start || !count || start >= sector_count || count > sector_count - start)
      return "readable; invalid partition bounds";
    if (!read(start, sector.data())) return "partition read failed";
    if (const char *format = sd_volume_signature(sector.data())) return format;
  }
  return "readable; format unidentified";
}
}
