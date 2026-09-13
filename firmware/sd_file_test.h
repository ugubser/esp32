#pragma once
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace lcars {
constexpr size_t SD_TEST_BYTES = 256 * 1024;
inline uint8_t sd_test_byte(size_t i) {
  uint32_t n = static_cast<uint32_t>(i) + 0x9e3779b9U;
  n ^= n >> 16; n *= 0x85ebca6bU; n ^= n >> 13;
  return static_cast<uint8_t>(n ^ (n >> 8));
}
inline std::string sd_verify_file(const std::string &path) {
  FILE *file = std::fopen(path.c_str(), "rb");
  if (!file) return "open for read failed";
  std::array<uint8_t, 1024> buffer{};
  std::string result;
  for (size_t offset = 0; offset < SD_TEST_BYTES && result.empty(); offset += buffer.size()) {
    if (std::fread(buffer.data(), 1, buffer.size(), file) != buffer.size()) {
      result = "short read"; break;
    }
    for (size_t i = 0; i < buffer.size(); ++i)
      if (buffer[i] != sd_test_byte(offset + i)) { result = "data mismatch"; break; }
  }
  if (result.empty() && (std::fgetc(file) != EOF || std::ferror(file))) result = "unexpected file length or read error";
  if (std::fclose(file) != 0 && result.empty()) result = "read close failed";
  return result;
}
inline std::string sd_prepare_test(const std::string &base) {
  const auto dir = base + "/LCRSTEST", temp = dir + "/DATA.TMP", saved = dir + "/DATA.BIN";
  // Exclusive directory ownership protects existing files and interrupted tests.
  if (::mkdir(dir.c_str(), 0700) != 0) return "test directory already exists or cannot be created";
  int fd = ::open(temp.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
  if (fd < 0) return "test file creation failed";
  std::array<uint8_t, 1024> buffer{};
  std::string result;
  for (size_t offset = 0; offset < SD_TEST_BYTES && result.empty(); offset += buffer.size()) {
    for (size_t i = 0; i < buffer.size(); ++i) buffer[i] = sd_test_byte(offset + i);
    size_t written = 0;
    while (written < buffer.size()) {
      auto count = ::write(fd, buffer.data() + written, buffer.size() - written);
      if (count <= 0) { result = "write failed"; break; }
      written += static_cast<size_t>(count);
    }
  }
  if (result.empty() && ::fsync(fd) != 0) result = "file sync failed";
  if (::close(fd) != 0 && result.empty()) result = "write close failed";
  if (!result.empty()) return result;
  result = sd_verify_file(temp);
  if (!result.empty()) return result;
  if (::rename(temp.c_str(), saved.c_str()) != 0) return "rename failed";
  struct stat st{};
  if (::stat(temp.c_str(), &st) == 0 || errno != ENOENT) return "old filename still present";
  return sd_verify_file(saved); // Leave only this known file for the reboot test.
}
inline std::string sd_finish_test(const std::string &base) {
  const auto dir = base + "/LCRSTEST", saved = dir + "/DATA.BIN";
  auto result = sd_verify_file(saved);
  if (!result.empty()) return result; // Preserve evidence if verification fails.
  if (::unlink(saved.c_str()) != 0) return "file deletion failed";
  struct stat st{};
  if (::stat(saved.c_str(), &st) == 0 || errno != ENOENT) return "deleted file still present";
  if (::rmdir(dir.c_str()) != 0) return "test directory deletion failed";
  return {};
}
}
