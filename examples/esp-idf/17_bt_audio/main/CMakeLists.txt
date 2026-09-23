set(ui_srcs "")
if(CONFIG_EXAMPLE_BT_UI_ENABLE)
    list(APPEND ui_srcs "bt_ui/bt_ui.c"
         "bt_ui/images/cis_stream_icon.c"
         "bt_ui/images/bis_stream_icon.c"
         "bt_ui/fonts/lv_font_notosanssc_regular_28.c")
endif()

set(reqs "bt" "nvs_flash" "console" "esp_codec_dev" "bsp_ues31s043h800v480c_u")
if(CONFIG_EXAMPLE_BT_UI_ENABLE)
    list(APPEND reqs "esp_lvgl_port" "lvgl" "esp_jpeg")
endif()

idf_component_register(SRCS "main.c" "pool_reg.c" "cmd_reg.c" "stream_proc.c" "bsp_gmf_bridge.c"
                    ${ui_srcs}
                    INCLUDE_DIRS "." "bt_ui"
                    REQUIRES ${reqs}
                    PRIV_REQUIRES esp_driver_jpeg
                    )
