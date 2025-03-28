set(incs )
set(srcs )

list(APPEND srcs
        vb6824.c
        )

list(APPEND incs
        .
        )

idf_component_register(SRCS "${srcs}"
                            INCLUDE_DIRS "${incs}"
                            REQUIRES driver esp_timer)