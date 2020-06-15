#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "driver/sdmmc_defs.h"
#include "sdmmc_cmd.h"
#include <Arduino.h>


int state = -1;

int log_init()
{
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();

    // To use 1-line SD mode, uncomment the following line:
    host.flags = SDMMC_HOST_FLAG_1BIT;
    host.max_freq_khz = SDMMC_FREQ_PROBING;

    // This initializes the slot without card detect (CD) and write protect (WP) signals.
    // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

    // Options for mounting the filesystem.
    // If format_if_mount_failed is set to true, SD card will be partitioned and formatted
    // in case when mounting fails.
    esp_vfs_fat_sdmmc_mount_config_t mount_config = 
    {
        .format_if_mount_failed = false,
        .max_files = 5
    };


    // Use settings defined above to initialize SD card and mount FAT filesystem.
    // Note: esp_vfs_fat_sdmmc_mount is an all-in-one convenience function.
    // Please check its source code and implement error recovery when developing
    // production applications.
    sdmmc_card_t* card;
    esp_err_t ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card);
    if (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
        {
            Serial.println("\n>>   Failed to mount filesystem. If you want the card to be formatted, set format_if_mount_failed = true.\n");
            state = -2;
        }
        else
        {
            Serial.printf("\n>>   Failed to initialize the card (%d). Make sure SD card lines have pull-up resistors in place.\n", ret);
        }

        state = -3;
        return state;
    }

    else
    {
         Serial.println("\n>>   SD: Filesystem mounted successful.\n");
         state = 0;
    }

    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, card);



    return 0;
}

int log_getState()
{
    return state;
}

void log_deinit()
{
    esp_vfs_fat_sdmmc_unmount();
}