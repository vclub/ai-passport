idf_component_register(
    SRCS "main.c"
         "buddy_app.cpp"
         "buddy_gfx.cpp"
         "buddy_protocol.c"
         "buddy_ble.c"
         "fonts/lv_font_cjk_12.c"
    INCLUDE_DIRS "."
                 "mascots"
    REQUIRES bsp nvs_flash bt esp_timer lvgl
)
