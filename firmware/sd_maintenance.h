#pragma once
#include "sd_probe.h"
#include "sd_file_test.h"
#include "esp_vfs_fat.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <atomic>

namespace lcars {
class SdMaintenance {
 public:
  void start(const std::string &command) {
    if (busy_) return;
    if (command != "FORMAT_EXFAT_TO_FAT32" && command != "VERIFY_AFTER_REBOOT" && command != "TEST_STORAGE") return;
    if (command == "FORMAT_EXFAT_TO_FAT32" && sd_probe_status.find("exFAT signature") == std::string::npos) {
      sd_probe_status = "Format refused: expected the detected exFAT card";
      return;
    }
    command_ = command;
    busy_ = true;
    sd_probe_status = "SD maintenance running";
    if (xTaskCreate([](void *arg) {
          auto *self = static_cast<SdMaintenance *>(arg);
          self->result_ = self->run();
          self->done_.store(true, std::memory_order_release);
          vTaskDelete(nullptr);
        }, "sd_maintenance", 8192, this, 1, nullptr) != pdPASS) {
      busy_ = false;
      sd_probe_status = "SD maintenance task creation failed";
    }
  }
  bool poll() {
    if (!done_.load(std::memory_order_acquire)) return false;
    sd_probe_status = result_;
    done_.store(false, std::memory_order_relaxed);
    busy_ = false;
    ESP_LOGI("sd_maintenance", "%s", sd_probe_status.c_str());
    return true;
  }
 private:
  bool busy_{};
  std::atomic<bool> done_{false};
  std::string command_, result_;
  std::string run() {
    const bool format = command_ == "FORMAT_EXFAT_TO_FAT32";
    const bool verify = command_ == "VERIFY_AFTER_REBOOT";
    spi_bus_config_t bus{};
    bus.mosi_io_num = 11; bus.miso_io_num = 13; bus.sclk_io_num = 12;
    bus.quadwp_io_num = -1; bus.quadhd_io_num = -1; bus.max_transfer_sz = 4096;
    auto err = spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) return std::string("SPI initialization failed: ") + esp_err_to_name(err);
    auto host = sdmmc_host_t SDSPI_HOST_DEFAULT();
    host.max_freq_khz = 10000;
    host.command_timeout_ms = 1000;
    auto device = sdspi_device_config_t SDSPI_DEVICE_CONFIG_DEFAULT();
    device.host_id = SPI2_HOST; device.gpio_cs = GPIO_NUM_10;
    esp_vfs_fat_mount_config_t config{};
    // Only the explicit format command may initialize a filesystem. Never on boot.
    config.format_if_mount_failed = format;
    config.max_files = 3; config.allocation_unit_size = 16384;
    sdmmc_card_t *card = nullptr;
    err = esp_vfs_fat_sdspi_mount("/sdcard", &host, &device, &config, &card);
    std::string result;
    if (err == ESP_OK) {
      const auto signature = sd_inspect(card->csd.capacity, [&](uint64_t lba, uint8_t *buffer) {
        return sdmmc_read_sectors(card, buffer, lba, 1) == ESP_OK;
      });
      if (std::string(signature) != "FAT32 signature") {
        result = std::string("Unexpected format: ") + signature;
      } else {
        auto failure = verify ? sd_finish_test("/sdcard") : sd_prepare_test("/sdcard");
        if (failure.empty()) {
          result = verify ? "PASS: FAT32; reboot readback 262144 bytes; deletion verified"
                          : "PASS: FAT32; write/read/rename 262144 bytes; reboot check pending";
        } else result = std::string("FAIL: FAT32; ") + failure;
      }
      err = esp_vfs_fat_sdcard_unmount("/sdcard", card);
      if (err != ESP_OK) result = std::string("Unmount failed: ") + esp_err_to_name(err);
    } else result = std::string("SD mount/format failed: ") + esp_err_to_name(err);
    err = spi_bus_free(SPI2_HOST);
    if (err != ESP_OK) result = std::string("SPI release failed: ") + esp_err_to_name(err);
    return result;
  }
};
inline SdMaintenance sd_maintenance;
}
