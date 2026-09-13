#include "../firmware/sd_file_test.h"
#include <cassert>
#include <filesystem>
#include <iostream>
int main() {
  char path[] = "/tmp/lcars-sd-test-XXXXXX";
  assert(mkdtemp(path));
  const std::string base = path, saved = base + "/LCRSTEST/DATA.BIN";
  assert(lcars::sd_prepare_test(base).empty());
  assert(std::filesystem::file_size(saved) == lcars::SD_TEST_BYTES);
  assert(!lcars::sd_prepare_test(base).empty()); // Never overwrite an existing test.
  assert(lcars::sd_verify_file(saved).empty());
  auto *f = std::fopen(saved.c_str(), "r+b"); assert(f);
  std::fseek(f, 1051, SEEK_SET);std::fputc(lcars::sd_test_byte(1051)^255,f);std::fclose(f);
  assert(lcars::sd_finish_test(base) == "data mismatch");
  assert(std::filesystem::exists(saved)); // Failed verification leaves data intact.
  f=std::fopen(saved.c_str(), "r+b");assert(f);
  std::fseek(f,1051,SEEK_SET);std::fputc(lcars::sd_test_byte(1051),f);std::fclose(f);
  assert(lcars::sd_finish_test(base).empty());
  assert(!std::filesystem::exists(base+"/LCRSTEST"));
  assert(lcars::sd_finish_test(base)=="open for read failed");
  assert(lcars::sd_prepare_test(base).empty());
  assert(::truncate(saved.c_str(),1024)==0);
  assert(lcars::sd_finish_test(base)=="short read");
  std::filesystem::remove_all(base);
  std::cout<<"SD file tests passed: complete readback, rename, exclusive creation, corruption/truncation detection, deletion\n";
}
