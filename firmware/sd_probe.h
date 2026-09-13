#pragma once
#include "sd_signature.h"
#include "driver/spi_master.h"
#include "driver/sdspi_host.h"
#include "sdmmc_cmd.h"
#include <cstdio>
#include <string>

namespace lcars {
inline std::string sd_probe_status = "Not checked";
inline void probe_sd_read_only() {
  // FNK0115Q vendor SD example: CS 10, SCLK 12, MISO 13, MOSI 11.
  spi_bus_config_t bus{};
  bus.mosi_io_num = 11;
  bus.miso_io_num = 13;
  bus.sclk_io_num = 12;
  bus.quadwp_io_num = -1;
  bus.quadhd_io_num = -1;
  bus.max_transfer_sz = 512;
  auto err = spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO);
  if (err != ESP_OK) {
    sd_probe_status = std::string("SPI initialization failed: ") + esp_err_to_name(err);
    return;
  }
  auto host = sdmmc_host_t SDSPI_HOST_DEFAULT();
  host.max_freq_khz = 10000;
  host.command_timeout_ms = 1000;
  auto device = sdspi_device_config_t SDSPI_DEVICE_CONFIG_DEFAULT();
  device.host_id = SPI2_HOST;
  device.gpio_cs = GPIO_NUM_10;
  sdspi_dev_handle_t handle{};
  err = sdspi_host_init_device(&device, &handle);
  if (err == ESP_OK) {
    host.slot = handle;
    sdmmc_card_t card{};
    err = sdmmc_card_init(&host, &card);
    if (err == ESP_OK) {
      char capacity[64];
      std::snprintf(capacity, sizeof(capacity), "Detected %.2f GB; ",
                    double(card.csd.capacity) * card.csd.sector_size / 1e9);
      sd_probe_status = capacity;
      if (card.csd.sector_size != 512) {
        sd_probe_status += "unsupported sector size for inspection";
      } else {
        sd_probe_status += sd_inspect(card.csd.capacity, [&](uint64_t lba, uint8_t *buffer) {
          return sdmmc_read_sectors(&card, buffer, lba, 1) == ESP_OK;
        });
      }
    } else {
      sd_probe_status = std::string("Card initialization failed: ") + esp_err_to_name(err);
    }
    sdspi_host_remove_device(handle);
  } else {
    sd_probe_status = std::string("SD SPI device failed: ") + esp_err_to_name(err);
  }
  spi_bus_free(SPI2_HOST);
  ESP_LOGI("sd_probe", "%s (read-only check)", sd_probe_status.c_str());
}
}
