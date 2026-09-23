idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_driver_jpeg esp_lcd fatfs esp_timer)
