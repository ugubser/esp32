#include "../firmware/sd_signature.h"
#include <cassert>
#include <map>
#include <vector>
#include <iostream>
using Sector = std::array<uint8_t, 512>;
static Sector volume(const char *name, unsigned offset) {
  Sector s{}; s[510]=0x55; s[511]=0xaa;
  std::memcpy(s.data()+offset,name,8); return s;
}
static void le32(uint8_t *p,uint32_t n) {
  for(unsigned i=0;i<4;++i)p[i]=uint8_t(n>>(8*i));
}
int main() {
  std::map<uint64_t,Sector> disk;
  std::vector<uint64_t> reads;
  auto inspect=[&]() {
    reads.clear();
    return std::string(lcars::sd_inspect(10000,[&](uint64_t lba,uint8_t *out) {
      assert(lba<10000);reads.push_back(lba);
      auto it=disk.find(lba);if(it==disk.end())return false;
      std::memcpy(out,it->second.data(),512);return true;
    }));
  };
  assert(inspect()=="sector read failed");
  disk[0]={};assert(inspect()=="readable; format unidentified");
  disk[0]=volume("EXFAT   ",3);assert(inspect()=="exFAT signature");
  assert(reads==std::vector<uint64_t>{0});
  disk[0]=volume("FAT32   ",82);assert(inspect()=="FAT32 signature");
  disk[0]=volume("FAT16   ",54);assert(inspect()=="FAT16 signature");
  disk[0]=volume("        ",3);
  auto *entry=disk[0].data()+446;entry[4]=7;
  le32(entry+8,2048);le32(entry+12,7952);
  assert(inspect()=="partition read failed");
  disk[2048]=volume("EXFAT   ",3);assert(inspect()=="exFAT signature");
  assert((reads==std::vector<uint64_t>{0,2048}));
  le32(entry+12,7953);assert(inspect()=="readable; invalid partition bounds");
  assert(reads.size()==1);
  le32(entry+8,0xffffffff);assert(inspect()=="readable; invalid partition bounds");
  entry[4]=0xee;assert(inspect()=="readable; GPT (filesystem not inspected)");
  assert(reads.size()==1);
  disk[0][510]=0;assert(inspect()=="readable; format unidentified");
  assert(lcars::sd_volume_signature(disk[0].data())==nullptr);
  std::cout<<"SD metadata tests passed: format signatures, bounded reads, blank media, read failures, GPT identification\n";
}
