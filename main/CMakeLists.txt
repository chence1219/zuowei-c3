set(SOURCES "audio/audio_codec.cc"
            "audio/audio_service.cc"
            "audio/codecs/no_audio_codec.cc"
            "audio/codecs/box_audio_codec.cc"
            "audio/codecs/es8311_audio_codec.cc"
            "audio/codecs/es8374_audio_codec.cc"
            "audio/codecs/es8388_audio_codec.cc"
            "audio/codecs/es8389_audio_codec.cc"
            "audio/codecs/dummy_audio_codec.cc"
            "audio/processors/audio_debugger.cc"
            "led/single_led.cc"
            "led/circular_strip.cc"
            "led/gpio_led.cc"
            "display/display.cc"
            "display/lcd_display.cc"
            "display/oled_display.cc"
            "protocols/protocol.cc"
            "protocols/mqtt_protocol.cc"
            "protocols/websocket_protocol.cc"
            "mcp_server.cc"
            "system_info.cc"
            "application.cc"
            "ota.cc"
            "custom_ota.cc"
            "settings.cc"
            "device_state_event.cc"
            "main.cc"
            )

set(INCLUDE_DIRS "." "display" "audio" "protocols")

# 添加板级公共文件
file(GLOB BOARD_COMMON_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/boards/common/*.cc)
list(APPEND SOURCES ${BOARD_COMMON_SOURCES})
list(APPEND INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/boards/common)

if(CONFIG_BOARD_PATH)
get_filename_component(BOARD_TYPE ${CONFIG_BOARD_PATH} NAME)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/boards/${CONFIG_BOARD_PATH}/CMakeLists.txt)
    message(STATUS "include ${CMAKE_CURRENT_SOURCE_DIR}/boards/${CONFIG_BOARD_PATH}/CMakeLists.txt")
    set(BOARD_PATH ${CMAKE_CURRENT_SOURCE_DIR}/boards/${CONFIG_BOARD_PATH})
    include(${CMAKE_CURRENT_SOURCE_DIR}/boards/${CONFIG_BOARD_PATH}/CMakeLists.txt)
else()
    file(GLOB BOARD_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/boards/${CONFIG_BOARD_PATH}/*.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/boards/${CONFIG_BOARD_PATH}/*.c
    )
    list(APPEND BOARD_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/boards/${CONFIG_BOARD_PATH})

    list(APPEND SOURCES ${BOARD_SOURCES})
    list(APPEND INCLUDE_DIRS ${BOARD_INCLUDE_DIRS})
endif()
endif()

if(CONFIG_USE_AUDIO_PROCESSOR)
    list(APPEND SOURCES "audio/processors/afe_audio_processor.cc")
else()
    list(APPEND SOURCES "audio/processors/no_audio_processor.cc")
endif()
if(CONFIG_USE_AFE_WAKE_WORD)
    list(APPEND SOURCES "audio/wake_words/afe_wake_word.cc")
elseif(CONFIG_USE_ESP_WAKE_WORD)
    list(APPEND SOURCES "audio/wake_words/esp_wake_word.cc")
elseif(CONFIG_USE_CUSTOM_WAKE_WORD)
    list(APPEND SOURCES "audio/wake_words/custom_wake_word.cc")
endif()

# 根据Kconfig选择语言目录
if(CONFIG_LANGUAGE_ZH_CN)
    set(LANG_DIR "zh-CN")
elseif(CONFIG_LANGUAGE_ZH_TW)
    set(LANG_DIR "zh-TW")
elseif(CONFIG_LANGUAGE_EN_US)
    set(LANG_DIR "en-US")
elseif(CONFIG_LANGUAGE_JA_JP)
    set(LANG_DIR "ja-JP")
elseif(CONFIG_LANGUAGE_VI_VN)
    set(LANG_DIR "vi-VN")
elseif(CONFIG_LANGUAGE_ID_ID)
    set(LANG_DIR "id-ID")
elseif(CONFIG_LANGUAGE_AR_SA)
    set(LANG_DIR "ar-SA")
endif()

# 定义生成路径
if(NOT CONFIG_ASSETS_PATH)
    set(CONFIG_ASSETS_PATH "assets")
else()
    list(APPEND INCLUDE_DIRS ${CONFIG_ASSETS_PATH}/..)
endif()
set(LANG_JSON "${CMAKE_CURRENT_SOURCE_DIR}/${CONFIG_ASSETS_PATH}/${LANG_DIR}/language.json")
set(LANG_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/${CONFIG_ASSETS_PATH}/lang_config.h")
file(GLOB LANG_SOUNDS ${CMAKE_CURRENT_SOURCE_DIR}/${CONFIG_ASSETS_PATH}/${LANG_DIR}/*.p3)
file(GLOB COMMON_SOUNDS ${CMAKE_CURRENT_SOURCE_DIR}/${CONFIG_ASSETS_PATH}/common/*.p3)

if(NOT CONFIG_ASSETS_FIX_PATH STREQUAL "")
    set(FIX_LANG_JSON "${CMAKE_CURRENT_SOURCE_DIR}/${CONFIG_ASSETS_FIX_PATH}/${LANG_DIR}/language.json")
    file(GLOB FIX_LANG_SOUNDS ${CMAKE_CURRENT_SOURCE_DIR}/${CONFIG_ASSETS_FIX_PATH}/${LANG_DIR}/*.p3)
    file(GLOB FIX_COMMON_SOUNDS ${CMAKE_CURRENT_SOURCE_DIR}/${CONFIG_ASSETS_FIX_PATH}/common/*.p3)

    foreach(fix_sound ${FIX_LANG_SOUNDS})
        get_filename_component(fix_sound_name ${fix_sound} NAME)
        foreach(sound ${LANG_SOUNDS})
            get_filename_component(sound_name ${sound} NAME)
            if(fix_sound_name STREQUAL sound_name)
                list(REMOVE_ITEM LANG_SOUNDS ${sound})
                break()
            endif()
        endforeach()
    endforeach()

    foreach(fix_sound ${FIX_COMMON_SOUNDS})
        get_filename_component(fix_sound_name ${fix_sound} NAME)
        foreach(sound ${COMMON_SOUNDS})
            get_filename_component(sound_name ${sound} NAME)
            if(fix_sound_name STREQUAL sound_name)
                list(REMOVE_ITEM COMMON_SOUNDS ${sound})
                break() 
            endif()
        endforeach()
    endforeach()

    list(APPEND LANG_SOUNDS ${FIX_LANG_SOUNDS})
    list(APPEND COMMON_SOUNDS ${FIX_COMMON_SOUNDS})

    list(APPEND INCLUDE_DIRS ${CONFIG_ASSETS_FIX_PATH}/..)
endif()

list(APPEND SOURCES "audio/codecs/vb6824_audio_codec.cc")
if(CONFIG_USE_AUDIO_CODEC_ENCODE_OPUS OR CONFIG_USE_AUDIO_CODEC_DECODE_OPUS)
list(REMOVE_ITEM SOURCES "audio/codecs/box_audio_codec.cc"
                        "audio/codecs/no_audio_codec.cc"
                        "audio/codecs/box_audio_codec.cc"
                        "audio/codecs/es8311_audio_codec.cc"
                        "audio/codecs/es8374_audio_codec.cc"
                        "audio/codecs/es8388_audio_codec.cc"
                        "audio/codecs/es8389_audio_codec.cc")
endif()

idf_component_register(SRCS ${SOURCES}
                    EMBED_FILES ${LANG_SOUNDS} ${COMMON_SOUNDS}
                    INCLUDE_DIRS ${INCLUDE_DIRS}
                    WHOLE_ARCHIVE
                    )

# 使用 target_compile_definitions 来定义 BOARD_TYPE, BOARD_NAME
# 如果 BOARD_NAME 为空，则使用 BOARD_TYPE
if(NOT BOARD_NAME)
    set(BOARD_NAME ${BOARD_TYPE})
endif()
target_compile_definitions(${COMPONENT_LIB}
                    PRIVATE BOARD_TYPE=\"${BOARD_TYPE}\" BOARD_NAME=\"${BOARD_NAME}\"
                    )

if(NOT CONFIG_ASSETS_FIX_PATH STREQUAL "")
add_custom_command(
    OUTPUT ${LANG_HEADER}
    COMMAND python ${PROJECT_DIR}/scripts/gen_lang_fix.py
            --input "${LANG_JSON}"
            --input_fix "${FIX_LANG_JSON}"
            --output "${LANG_HEADER}"
    DEPENDS
        ${LANG_JSON}
        ${PROJECT_DIR}/scripts/gen_lang_fix.py
    COMMENT "Generating ${LANG_DIR} language config"
)
else()
# 添加生成规则
add_custom_command(
    OUTPUT ${LANG_HEADER}
    COMMAND python ${PROJECT_DIR}/scripts/gen_lang.py
            --input "${LANG_JSON}"
            --output "${LANG_HEADER}"
    DEPENDS
        ${LANG_JSON}
        ${PROJECT_DIR}/scripts/gen_lang.py
    COMMENT "Generating ${LANG_DIR} language config"
)
endif()

# 强制建立生成依赖
add_custom_target(lang_header ALL
    DEPENDS ${LANG_HEADER}
)

if(CONFIG_BOARD_TYPE_ESP_HI)
set(URL "https://github.com/espressif2022/image_player/raw/main/test_apps/test_8bit")
set(SPIFFS_DIR "${CMAKE_BINARY_DIR}/emoji")
file(MAKE_DIRECTORY ${SPIFFS_DIR})

# List all files to download
set(FILES_TO_DOWNLOAD "")
list(APPEND FILES_TO_DOWNLOAD "Anger_enter.aaf" "Anger_loop.aaf" "Anger_return.aaf")
list(APPEND FILES_TO_DOWNLOAD "happy_enter.aaf" "happy_loop.aaf" "happ_return.aaf")
list(APPEND FILES_TO_DOWNLOAD "sad_enter.aaf" "sad_loop.aaf" "sad_return.aaf")
list(APPEND FILES_TO_DOWNLOAD "scorn_enter.aaf" "scorn_loop.aaf" "scorn_return.aaf")
list(APPEND FILES_TO_DOWNLOAD "left_enter.aaf" "left_loop.aaf" "left_return.aaf")
list(APPEND FILES_TO_DOWNLOAD "right_enter.aaf" "right_loop.aaf" "right_return.aaf")
list(APPEND FILES_TO_DOWNLOAD "asking.aaf" "blink_once.aaf" "blink_quick.aaf")
list(APPEND FILES_TO_DOWNLOAD "connecting.aaf" "panic_enter.aaf" "panic_loop.aaf")
list(APPEND FILES_TO_DOWNLOAD "panic_return.aaf" "wake.aaf")

foreach(FILENAME IN LISTS FILES_TO_DOWNLOAD)
    set(REMOTE_FILE "${URL}/${FILENAME}")
    set(LOCAL_FILE "${SPIFFS_DIR}/${FILENAME}")
    
    # 检查本地文件是否存在
    if(EXISTS ${LOCAL_FILE})
        message(STATUS "File ${FILENAME} already exists, skipping download")
    else()
        message(STATUS "Downloading ${FILENAME}")
        file(DOWNLOAD ${REMOTE_FILE} ${LOCAL_FILE}
             STATUS DOWNLOAD_STATUS)
        list(GET DOWNLOAD_STATUS 0 STATUS_CODE)
        if(NOT STATUS_CODE EQUAL 0)
            message(FATAL_ERROR "Failed to download ${FILENAME} from ${URL}")
        endif()
    endif()
endforeach()

spiffs_create_partition_assets(
    assets_A
    ${SPIFFS_DIR}
    FLASH_IN_PROJECT
    MMAP_FILE_SUPPORT_FORMAT ".aaf"
)
endif()