set(incs )
set(srcs )

list(APPEND srcs
        rbuffer/C/rbuffer.c
        )

list(APPEND incs
        rbuffer/C
        )

idf_component_register(SRCS "${srcs}"
                            INCLUDE_DIRS "${incs}")