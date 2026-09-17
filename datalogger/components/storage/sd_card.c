// implements
#include "storage.h"

// system includes
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/spi_master.h"
#include "driver/sdspi_host.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "ff.h"

#define MOUNT_POINT "/sdcard"
#define SPI_DMA_CHAN SPI_DMA_CH_AUTO

static const char *TAG = "sdcard";

static sdmmc_card_t *card = NULL;

esp_err_t sdcard_init(gpio_num_t pin_num_clk, gpio_num_t pin_num_cs, gpio_num_t pin_num_miso, gpio_num_t pin_num_mosi)
{
    esp_err_t ret;

    gpio_hold_dis(pin_num_clk);
    gpio_hold_dis(pin_num_cs);
    gpio_hold_dis(pin_num_miso);
    gpio_hold_dis(pin_num_mosi);

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.max_freq_khz = 10000;

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = pin_num_mosi,
        .miso_io_num = pin_num_miso,
        .sclk_io_num = pin_num_clk,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };

    gpio_set_level(pin_num_cs, 1);
    

    ret = spi_bus_initialize(host.slot, &bus_cfg, SPI_DMA_CHAN);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
        return ret;
    } else {
        ESP_LOGI(TAG, "SPI Bus initilized");
    }

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = pin_num_cs;
    slot_config.host_id = host.slot;
    ESP_LOGI(TAG, "SPI device config created");

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024,
    };

    ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SD card: %s", esp_err_to_name(ret));
        spi_bus_free(host.slot);
        return ret;
    }

    sdmmc_card_print_info(stdout, card);
    ESP_LOGI(TAG, "SD card mounted at %s", MOUNT_POINT);
    return ESP_OK;
}

void get_sdcard_usage(uint64_t *free, uint64_t *used, uint64_t *total) {
    FATFS *fs;
    DWORD free_clusters;
    FRESULT res;

    res = f_getfree(MOUNT_POINT, &free_clusters, &fs);
    if (res != FR_OK) {
        *free = 0;
        *total = 0;
        *used = 0;
        return;
    }

    // required, because if the sector size is fixed fs->ssize does not exist and if it is not fixed, the mutable variable size is needed
#if FF_MAX_SS != FF_MIN_SS
    uint64_t sector_size = fs->ssize;
#else
    uint64_t sector_size = FF_MIN_SS;
#endif

    uint64_t total_clusters = (uint64_t)(fs->n_fatent - 2);
    uint64_t cluster_size = (uint64_t)fs->csize * sector_size;

    *free = free_clusters * cluster_size / 1024.0 / 1024.0;
    *total = total_clusters * cluster_size / 1024.0 / 1024.0;
    *used = *total - *free;
}

void sdcard_deinit(void)
{
    if (card == NULL) return;

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    esp_vfs_fat_sdcard_unmount(MOUNT_POINT, card);
    spi_bus_free(host.slot);
    card = NULL;

    ESP_LOGI(TAG, "SD card unmounted");
}