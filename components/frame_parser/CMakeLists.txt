set(incs )
set(srcs )

list(APPEND srcs
        frame_parser.c
        )

list(APPEND incs
        .
        )

idf_component_register(SRCS "${srcs}"
                            INCLUDE_DIRS "${incs}"
                            REQUIRES rbuffer )