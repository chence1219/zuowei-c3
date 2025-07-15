file(GLOB fonts "*.c")

list(APPEND srcs ${fonts})

list(APPEND incs .)



idf_component_register(SRCS "font_noto_thin_14_1.c" "font_noto_regular_14_4.c" "font_noto_regular_14_1.c" "font_noto_thin_16_4.c" "font_noto_thin_16_1.c" "font_noto_regular_16_4.c" "font_noto_regular_16_1.c" "${srcs}"
    INCLUDE_DIRS "${incs}"
    REQUIRES lvgl)



