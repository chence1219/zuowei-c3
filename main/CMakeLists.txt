# Define default assets files (Absolute url starting with http or https is supported)
set(ASSETS_URL_PREFIX "https://files.xiaozhi.me/assets/default/")

set(ASSETS_PUHUI_COMMON_14_1                    "${ASSETS_URL_PREFIX}none-font_puhui_common_14_1-none.bin")
set(ASSETS_XIAOZHI_WAKENET_ONLY                 "${ASSETS_URL_PREFIX}wn9_nihaoxiaozhi_tts-none-none.bin")
set(ASSETS_XIAOZHI_PUHUI_COMMON_14_1            "${ASSETS_URL_PREFIX}wn9_nihaoxiaozhi_tts-font_puhui_common_14_1-none.bin")
set(ASSETS_XIAOZHI_PUHUI_COMMON_16_4_EMOJI_32   "${ASSETS_URL_PREFIX}wn9_nihaoxiaozhi_tts-font_puhui_common_16_4-emojis_32.bin")
set(ASSETS_XIAOZHI_PUHUI_COMMON_16_4_EMOJI_64   "${ASSETS_URL_PREFIX}wn9_nihaoxiaozhi_tts-font_puhui_common_16_4-emojis_64.bin")
set(ASSETS_XIAOZHI_PUHUI_COMMON_20_4_EMOJI_64   "${ASSETS_URL_PREFIX}wn9_nihaoxiaozhi_tts-font_puhui_common_20_4-emojis_64.bin")
set(ASSETS_XIAOZHI_PUHUI_COMMON_30_4_EMOJI_64   "${ASSETS_URL_PREFIX}wn9_nihaoxiaozhi_tts-font_puhui_common_30_4-emojis_64.bin")
set(ASSETS_XIAOZHI_S_WAKENET_ONLY               "${ASSETS_URL_PREFIX}wn9s_nihaoxiaozhi-none-none.bin")
set(ASSETS_XIAOZHI_S_PUHUI_COMMON_14_1          "${ASSETS_URL_PREFIX}wn9s_nihaoxiaozhi-font_puhui_common_14_1-none.bin")
set(ASSETS_XIAOZHI_S_PUHUI_COMMON_16_4_EMOJI_32 "${ASSETS_URL_PREFIX}wn9s_nihaoxiaozhi-font_puhui_common_16_4-emojis_32.bin")
set(ASSETS_XIAOZHI_S_PUHUI_COMMON_20_4_EMOJI_32 "${ASSETS_URL_PREFIX}wn9s_nihaoxiaozhi-font_puhui_common_20_4-emojis_32.bin")
set(ASSETS_XIAOZHI_S_PUHUI_COMMON_20_4_EMOJI_64 "${ASSETS_URL_PREFIX}wn9s_nihaoxiaozhi-font_puhui_common_20_4-emojis_64.bin")
set(ASSETS_XIAOZHI_S_PUHUI_COMMON_30_4_EMOJI_64 "${ASSETS_URL_PREFIX}wn9s_nihaoxiaozhi-font_puhui_common_30_4-emojis_64.bin")

# Embedded font files defined in `xiaozhi-fonts` component
# Basic fonts include ASCII and about 600 characters used in assets/locales
set(FONT_PUHUI_BASIC_14_1 font_puhui_basic_14_1)
set(FONT_PUHUI_BASIC_16_4 font_puhui_basic_16_4)
set(FONT_PUHUI_BASIC_20_4 font_puhui_basic_20_4)
set(FONT_PUHUI_BASIC_30_4 font_puhui_basic_30_4)
# Common fonts include about 7000 common characters generated with DeepSeek R1 tokenizer
set(FONT_PUHUI_COMMON_14_1 font_puhui_14_1)
set(FONT_PUHUI_COMMON_16_4 font_puhui_16_4)
set(FONT_PUHUI_COMMON_20_4 font_puhui_20_4)
set(FONT_PUHUI_COMMON_30_4 font_puhui_30_4)
set(FONT_AWESOME_14_1 font_awesome_14_1)
set(FONT_AWESOME_30_1 font_awesome_30_1)
set(FONT_AWESOME_16_4 font_awesome_16_4)
set(FONT_AWESOME_20_4 font_awesome_20_4)
set(FONT_AWESOME_30_4 font_awesome_30_4)


# Define source files
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
            "display/lvgl_display/lvgl_display.cc"
            "display/lvgl_display/emoji_collection.cc"
            "display/lvgl_display/lvgl_theme.cc"
            "display/lvgl_display/lvgl_font.cc"
            "display/lvgl_display/lvgl_image.cc"
            "display/lvgl_display/gif/lvgl_gif.cc"
            "display/lvgl_display/gif/gifdec.c"
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
            "assets.cc"
            "main.cc"
            )

set(INCLUDE_DIRS "." "display" "display/lvgl_display" "audio" "protocols")

# Add board common files
file(GLOB BOARD_COMMON_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/boards/common/*.cc)
list(APPEND SOURCES ${BOARD_COMMON_SOURCES})
list(APPEND INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/boards/common)

# 允许添加外部代码
if(EXTERNAL_SOURCE_PATH)
    file(GLOB EXTERNAL_SOURCES
        ${EXTERNAL_SOURCE_PATH}/*.cc
        ${EXTERNAL_SOURCE_PATH}/*.c
    )
    list(APPEND SOURCES ${EXTERNAL_SOURCES})
    list(APPEND INCLUDE_DIRS ${EXTERNAL_SOURCE_PATH}/../)
endif()

# Set default LVGL_TEXT_FONT and LVGL_ICON_FONT
set(LVGL_TEXT_FONT ${FONT_PUHUI_COMMON_16_4})
set(LVGL_ICON_FONT ${FONT_AWESOME_16_4})

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

# Select audio processor according to Kconfig
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

# Select language directory according to Kconfig
if(CONFIG_LANGUAGE_ZH_CN)
    set(LANG_DIR "zh-CN")
elseif(CONFIG_LANGUAGE_ZH_TW)
    set(LANG_DIR "zh-TW")
elseif(CONFIG_LANGUAGE_EN_US)
    set(LANG_DIR "en-US")
elseif(CONFIG_LANGUAGE_JA_JP)
    set(LANG_DIR "ja-JP")
elseif(CONFIG_LANGUAGE_KO_KR)
    set(LANG_DIR "ko-KR")
elseif(CONFIG_LANGUAGE_VI_VN)
    set(LANG_DIR "vi-VN")
elseif(CONFIG_LANGUAGE_TH_TH)
    set(LANG_DIR "th-TH")
elseif(CONFIG_LANGUAGE_DE_DE)
    set(LANG_DIR "de-DE")
elseif(CONFIG_LANGUAGE_FR_FR)
    set(LANG_DIR "fr-FR")
elseif(CONFIG_LANGUAGE_ES_ES)
    set(LANG_DIR "es-ES")
elseif(CONFIG_LANGUAGE_IT_IT)
    set(LANG_DIR "it-IT")
elseif(CONFIG_LANGUAGE_RU_RU)
    set(LANG_DIR "ru-RU")
elseif(CONFIG_LANGUAGE_AR_SA)
    set(LANG_DIR "ar-SA")
elseif(CONFIG_LANGUAGE_HI_IN)
    set(LANG_DIR "hi-IN")
elseif(CONFIG_LANGUAGE_PT_PT)
    set(LANG_DIR "pt-PT")
elseif(CONFIG_LANGUAGE_PL_PL)
    set(LANG_DIR "pl-PL")
elseif(CONFIG_LANGUAGE_CS_CZ)
    set(LANG_DIR "cs-CZ")
elseif(CONFIG_LANGUAGE_FI_FI)
    set(LANG_DIR "fi-FI")
elseif(CONFIG_LANGUAGE_TR_TR)
    set(LANG_DIR "tr-TR")
elseif(CONFIG_LANGUAGE_ID_ID)
    set(LANG_DIR "id-ID")
elseif(CONFIG_LANGUAGE_UK_UA)
    set(LANG_DIR "uk-UA")
elseif(CONFIG_LANGUAGE_RO_RO)
    set(LANG_DIR "ro-RO")
endif()




# 定义生成路径
if(NOT CONFIG_ASSETS_PATH)
    set(CONFIG_ASSETS_PATH "assets")
else()
    list(APPEND INCLUDE_DIRS ${CONFIG_ASSETS_PATH}/..)
endif()

set(LANG_JSON "${CMAKE_CURRENT_SOURCE_DIR}/${CONFIG_ASSETS_PATH}/locales/${LANG_DIR}/language.json")
set(LANG_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/${CONFIG_ASSETS_PATH}/lang_config.h")
file(GLOB LANG_SOUNDS ${CMAKE_CURRENT_SOURCE_DIR}/${CONFIG_ASSETS_PATH}/locales/${LANG_DIR}/*.ogg)
file(GLOB COMMON_SOUNDS ${CMAKE_CURRENT_SOURCE_DIR}/${CONFIG_ASSETS_PATH}/common/*.ogg)

if(NOT CONFIG_ASSETS_FIX_PATH STREQUAL "")
    set(FIX_LANG_JSON "${CMAKE_CURRENT_SOURCE_DIR}/${CONFIG_ASSETS_FIX_PATH}/${LANG_DIR}/language.json")
    file(GLOB FIX_LANG_SOUNDS ${CMAKE_CURRENT_SOURCE_DIR}/${CONFIG_ASSETS_FIX_PATH}/${LANG_DIR}/*.ogg)
    file(GLOB FIX_COMMON_SOUNDS ${CMAKE_CURRENT_SOURCE_DIR}/${CONFIG_ASSETS_FIX_PATH}/common/*.ogg)

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

# Use target_compile_definitions to define BOARD_TYPE, BOARD_NAME
# If BOARD_NAME is empty, use BOARD_TYPE
if(NOT BOARD_NAME)
    set(BOARD_NAME ${BOARD_TYPE})
endif()
target_compile_definitions(${COMPONENT_LIB}
                    PRIVATE BOARD_TYPE=\"${BOARD_TYPE}\" BOARD_NAME=\"${BOARD_NAME}\"
                    PRIVATE DEFAULT_ASSETS=\"${DEFAULT_ASSETS}\" LVGL_TEXT_FONT=${LVGL_TEXT_FONT} LVGL_ICON_FONT=${LVGL_ICON_FONT}
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
            --language "${LANG_DIR}"
            --output "${LANG_HEADER}"
    DEPENDS
        ${LANG_JSON}
        ${PROJECT_DIR}/scripts/gen_lang.py
    COMMENT "Generating ${LANG_DIR} language config"
)
endif()

# Force build generation dependencies
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
    
    # Check if local file exists
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

if(CONFIG_BOARD_TYPE_ECHOEAR)

idf_build_get_property(build_components BUILD_COMPONENTS)
foreach(COMPONENT ${build_components})
    if(COMPONENT MATCHES "esp_emote_gfx" OR COMPONENT MATCHES "espressif2022__esp_emote_gfx")
        set(EMOTE_GFX_COMPONENT ${COMPONENT})
        idf_component_get_property(EMOTE_GFX_COMPONENT_PATH ${EMOTE_GFX_COMPONENT} COMPONENT_DIR)
        set(SPIFFS_DIR "${EMOTE_GFX_COMPONENT_PATH}/emoji_normal")
        break()
    endif()
endforeach()

spiffs_create_partition_assets(
    assets_A
    ${SPIFFS_DIR}
    FLASH_IN_PROJECT
    MMAP_FILE_SUPPORT_FORMAT ".aaf, ttf, bin"
    IMPORT_INC_PATH ${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD_TYPE}
)
endif()

# Font configuration validation function
function(validate_font_config board_name text_font default_assets)
    if(text_font)
        # Check if DEFAULT_ASSETS contains font
        if(default_assets AND default_assets MATCHES "font_")
            # Rule 1: If DEFAULT_ASSETS uses font, LVGL_TEXT_FONT must be BASIC
            if(NOT text_font MATCHES "basic")
                message(FATAL_ERROR "Font config error for ${board_name}: DEFAULT_ASSETS contains COMMON font but LVGL_TEXT_FONT is not BASIC (${text_font})")
            endif()
        else()
            # Rule 2: If no DEFAULT_ASSETS or DEFAULT_ASSETS doesn't contain font_, LVGL_TEXT_FONT must not be BASIC
            if(text_font MATCHES "basic")
                message(FATAL_ERROR "Font config error for ${board_name}: No DEFAULT_ASSETS with COMMON font but LVGL_TEXT_FONT is not COMMON (${text_font})")
            endif()
        endif()
        # Pass validation
        message(STATUS "Font config validation passed for ${board_name}: LVGL_TEXT_FONT=${text_font}, DEFAULT_ASSETS=${default_assets}")
    endif()
endfunction()

# DEFAULT_ASSETS prefix validation function
function(validate_default_assets_prefix board_name default_assets)
    if(default_assets)
        # Check for ESP32S3/P4 target - DEFAULT_ASSETS cannot start with "wn9s_"
        if(CONFIG_IDF_TARGET_ESP32S3 OR CONFIG_IDF_TARGET_ESP32P4)
            if(default_assets MATCHES "^wn9s_")
                message(FATAL_ERROR "Assets config error for ${board_name}: DEFAULT_ASSETS cannot start with 'wn9s_' for ESP32S3 target (${default_assets})")
            endif()
        endif()
        
        # Check for ESP32C3/C6 target - DEFAULT_ASSETS cannot start with "wn9_"
        if(CONFIG_IDF_TARGET_ESP32C3 OR CONFIG_IDF_TARGET_ESP32C6)
            if(default_assets MATCHES "^wn9_")
                message(FATAL_ERROR "Assets config error for ${board_name}: DEFAULT_ASSETS cannot start with 'wn9_' for ESP32C3/C6 target (${default_assets})")
            endif()
        endif()
        
        # Pass validation
        message(STATUS "Assets prefix validation passed for ${board_name}: DEFAULT_ASSETS=${default_assets}")
    endif()
endfunction()

# Global font configuration validation
# This will validate the current board's font configuration
if(LVGL_TEXT_FONT)
    validate_font_config("${BOARD_TYPE}" "${LVGL_TEXT_FONT}" "${DEFAULT_ASSETS}")
endif()

# Global DEFAULT_ASSETS prefix validation
# This will validate the current board's DEFAULT_ASSETS prefix configuration
if(DEFAULT_ASSETS)
    validate_default_assets_prefix("${BOARD_TYPE}" "${DEFAULT_ASSETS}")
endif()

# Function to get local assets file path (handles both URL and local file)
function(get_assets_local_file assets_source assets_local_file_var)
    # Check if it's a URL (starts with http:// or https://)
    if(assets_source MATCHES "^https?://")
        # It's a URL, download it
        get_filename_component(ASSETS_FILENAME "${assets_source}" NAME)
        set(ASSETS_LOCAL_FILE "${CMAKE_BINARY_DIR}/${ASSETS_FILENAME}")
        set(ASSETS_TEMP_FILE "${CMAKE_BINARY_DIR}/${ASSETS_FILENAME}.tmp")
        
        # Check if local file exists
        if(EXISTS ${ASSETS_LOCAL_FILE})
            message(STATUS "Assets file ${ASSETS_FILENAME} already exists, skipping download")
        else()
            message(STATUS "Downloading ${ASSETS_FILENAME}")
            
            # Clean up any existing temp file
            if(EXISTS ${ASSETS_TEMP_FILE})
                file(REMOVE ${ASSETS_TEMP_FILE})
            endif()
            
            # Download to temporary file first
            file(DOWNLOAD ${assets_source} ${ASSETS_TEMP_FILE}
                 STATUS DOWNLOAD_STATUS)
            list(GET DOWNLOAD_STATUS 0 STATUS_CODE)
            if(NOT STATUS_CODE EQUAL 0)
                # Clean up temp file on failure
                if(EXISTS ${ASSETS_TEMP_FILE})
                    file(REMOVE ${ASSETS_TEMP_FILE})
                endif()
                message(FATAL_ERROR "Failed to download ${ASSETS_FILENAME} from ${assets_source}")
            endif()
            
            # Move temp file to final location (atomic operation)
            file(RENAME ${ASSETS_TEMP_FILE} ${ASSETS_LOCAL_FILE})
            message(STATUS "Successfully downloaded ${ASSETS_FILENAME}")
        endif()
    else()
        # It's a local file path
        if(IS_ABSOLUTE "${assets_source}")
            set(ASSETS_LOCAL_FILE "${assets_source}")
        else()
            set(ASSETS_LOCAL_FILE "${CMAKE_CURRENT_SOURCE_DIR}/${assets_source}")
        endif()
        
        # Check if local file exists
        if(NOT EXISTS ${ASSETS_LOCAL_FILE})
            message(FATAL_ERROR "Assets file not found: ${ASSETS_LOCAL_FILE}")
        endif()
        
        message(STATUS "Using assets file: ${ASSETS_LOCAL_FILE}")
    endif()
    
    set(${assets_local_file_var} ${ASSETS_LOCAL_FILE} PARENT_SCOPE)
endfunction()

# Flash assets based on configuration
if(CONFIG_FLASH_DEFAULT_ASSETS)
    # Flash default assets
    if(DEFAULT_ASSETS)
        get_assets_local_file("${DEFAULT_ASSETS}" ASSETS_LOCAL_FILE)
        esptool_py_flash_to_partition(flash "assets" "${ASSETS_LOCAL_FILE}")
        message(STATUS "Default assets download and flash configured: ${DEFAULT_ASSETS} -> assets partition")
    else()
        message(WARNING "FLASH_DEFAULT_ASSETS is enabled but no DEFAULT_ASSETS is defined for board ${BOARD_TYPE}")
    endif()
elseif(CONFIG_FLASH_CUSTOM_ASSETS)
    # Flash custom assets
    get_assets_local_file("${CONFIG_CUSTOM_ASSETS_FILE}" ASSETS_LOCAL_FILE)
    esptool_py_flash_to_partition(flash "assets" "${ASSETS_LOCAL_FILE}")
    message(STATUS "Custom assets flash configured: ${ASSETS_LOCAL_FILE} -> assets partition")
elseif(CONFIG_FLASH_NONE_ASSETS)
    message(STATUS "Assets flashing disabled (FLASH_NONE_ASSETS)")
endif()
