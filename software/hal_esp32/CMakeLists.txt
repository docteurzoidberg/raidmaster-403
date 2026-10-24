# HAL de l'ESP32 (composant ESP-IDF, build firmware seulement) : pilotes de la carte Waveshare
# ESP32-S3-Touch-LCD-3.5B et implémentations des interfaces du core (core/include/hal/).
#
# GPS : trajet simulé (hal_gps_demo.c) tant que le pilote UART du NEO-M9N n'est pas écrit.
#
# WHOLE_ARCHIVE : core/ et ui/ appellent les fonctions HAL (hal_storage_*, hal_asset_*…) sans
# dépendre de ce composant (ils restent portables) ; lié en entier, il n'y a pas d'ordre
# d'édition de liens à respecter.
idf_component_register(
    WHOLE_ARCHIVE
    SRCS
        board.c
        board_display.c
        board_touch.c
        board_rtc.c
        board_pmu.c
        hal_time_esp.c
        hal_storage_nvs.c
        hal_gps_demo.c
        hal_asset_partition.c
    INCLUDE_DIRS "."
    REQUIRES core esp_driver_i2c esp_lcd
    PRIV_REQUIRES esp_driver_ledc esp_driver_spi esp_timer esp_partition nvs_flash esp_app_format
)
