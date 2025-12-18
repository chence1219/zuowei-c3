set(incs )
set(priv_incs )
set(srcs )
set(assets )
list(APPEND srcs
    vb_transport.c
    vb_procotol.c
    vb_api.c
    vb_adapter.c
    vb_audio.c
    vb_event.c
        )

list(APPEND incs inc)
list(APPEND priv_incs priv_inc)


idf_component_register(SRCS "${srcs}"
                            INCLUDE_DIRS "${incs}"
                            PRIV_INCLUDE_DIRS "${priv_incs}"
                            EMBED_TXTFILES "${assets}"                    
                            LDFRAGMENTS "link.lf"
                            REQUIRES driver esp_wifi esp_timer)


# if(CONFIG_IDF_TARGET_ESP32)
#     set(lib_target "esp32")
# elseif(CONFIG_IDF_TARGET_ESP32S2)
#     set(lib_target "esp32s2")
# elseif(CONFIG_IDF_TARGET_ESP32S3)
#     set(lib_target "esp32s3")
# elseif(CONFIG_IDF_TARGET_ESP32C2)
#     set(lib_target "esp32c2")
# elseif(CONFIG_IDF_TARGET_ESP32C3)
#     set(lib_target "esp32c3")
# endif()
#
#
# if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "5.5")
#     set(lib_version "idf5.5")
# else()
#     set(lib_version "idf5.4")
# endif()
#
# add_prebuilt_library(vbota "libs/${lib_version}/lib${lib_target}.a")
#
# target_link_libraries(${COMPONENT_LIB} PRIVATE vbota)