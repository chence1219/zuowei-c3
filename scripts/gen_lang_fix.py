#!/usr/bin/env python3
import argparse
import json
import os
import re
import sys
import gen_lang

HEADER_TEMPLATE = """// Auto-generated language config
// Language: {lang_code} with en-US fallback
#pragma once

#include <string_view>

#ifndef {lang_code_for_font}
    #define {lang_code_for_font}  // 預設語言
#endif

namespace Lang {{
    // 语言元数据
    constexpr const char* CODE = "{lang_code}";

    // 字符串资源 (en-US as fallback for missing keys)
    namespace Strings {{
{strings}
    }}

    // 音效资源 (en-US as fallback for missing audio files)
    namespace Sounds {{
{sounds}
    }}
}}
"""

def load_base_language(assets_dir):
    """加载 en-US 基准语言数据"""
    base_lang_path = os.path.join(assets_dir, 'locales', 'en-US', 'language.json')
    if os.path.exists(base_lang_path):
        try:
            with open(base_lang_path, 'r', encoding='utf-8') as f:
                base_data = json.load(f)
                print(f"Loaded base language en-US with {len(base_data.get('strings', {}))} strings")
                return base_data
        except json.JSONDecodeError as e:
            print(f"Warning: Failed to parse en-US language file: {e}")
    else:
        print("Warning: en-US base language file not found, fallback mechanism disabled")
    return {'strings': {}}

def get_sound_files(directory):
    """获取目录中的音效文件列表"""
    if not os.path.exists(directory):
        return []
    return [f for f in os.listdir(directory) if f.endswith('.ogg')]

def generate_header(input_path, input_fix_path, output_path):
    # 从输入路径推导语言代码和项目结构
    # input_path 通常是 assets/locales/zh-CN/language.json
    lang_dir = os.path.dirname(input_path)  # assets/locales/zh-CN
    lang_code = os.path.basename(lang_dir)  # zh-CN
    locales_dir = os.path.dirname(lang_dir)  # assets/locales
    assets_dir = os.path.dirname(locales_dir)  # assets
    input_fix_dir = os.path.dirname(os.path.dirname(input_fix_path))
    
    print(f"Processing language: {lang_code}")
    print(f"Input file path: {input_path}")
    print(f"Input fix path: {input_fix_path}")
    print(f"Output file path: {output_path}")
    
    if not os.path.exists(input_path):
        raise FileNotFoundError(f"Language file not found: {input_path}")
    
    with open(input_path, 'r', encoding='utf-8') as f:
        data = json.load(f)

    # 验证数据结构
    if 'language' not in data or 'strings' not in data:
        raise ValueError("Invalid JSON structure")

    # 加载 en-US 基准语言数据
    base_data = load_base_language(assets_dir)
    
    # 合并字符串：以 en-US 为基准，用户语言覆盖
    base_strings = base_data.get('strings', {})
    user_strings = data['strings']
    merged_strings = base_strings.copy()
    merged_strings.update(user_strings)
    
    # 统计信息
    base_count = len(base_strings)
    user_count = len(user_strings)
    total_count = len(merged_strings)
    fallback_count = total_count - user_count
    
    print(f"Language {lang_code} string statistics:")
    print(f"  - Base language (en-US): {base_count} strings")
    print(f"  - User language: {user_count} strings")
    print(f"  - Total: {total_count} strings")
    if fallback_count > 0:
        print(f"  - Fallback to en-US: {fallback_count} strings")

    # 生成字符串常量
    strings = []
    sounds = []
    for key, value in merged_strings.items():
        value = value.replace('"', '\\"')
        strings.append(f'        constexpr const char* {key.upper()} = "{value}";')

    # 收集音效文件：以 en-US 为基准，用户语言覆盖
    current_lang_dir = os.path.join(assets_dir, 'locales', lang_code)
    base_lang_dir = os.path.join(assets_dir, 'locales', 'en-US')
    common_dir = os.path.join(assets_dir, 'common')
    common_dir_fix = os.path.join(input_fix_dir, 'common')
    
    # 获取所有可能的音效文件
    base_sounds = get_sound_files(base_lang_dir)
    current_sounds = get_sound_files(current_lang_dir)
    common_sounds = get_sound_files(common_dir)
    common_sounds_fix = get_sound_files(common_dir_fix)
    
    # 处理 input_fix 目录中的额外音效文件
    input_fix_sounds = []
    if input_fix_path and os.path.exists(input_fix_path):
        input_fix_dir = os.path.dirname(input_fix_path)
        input_fix_sounds = get_sound_files(input_fix_dir)
        print(f"Found {len(input_fix_sounds)} additional sound files in input-fix directory")
    
    # 合并音效文件列表：用户语言覆盖基准语言
    all_sound_files = set(base_sounds)
    all_sound_files.update(current_sounds)
    all_sound_files.update(input_fix_sounds)  # 添加 input-fix 中的音效文件
    common_sounds_files = set(common_sounds)
    common_sounds_files.update(common_sounds_fix)  # 添加 input-fix 中的公共音效文件
    
    # 音效统计信息
    base_sound_count = len(base_sounds)
    user_sound_count = len(current_sounds)
    common_sound_count = len(common_sounds)
    input_fix_sound_count = len(input_fix_sounds)
    sound_fallback_count = len(set(base_sounds) - set(current_sounds))
    
    print(f"Language {lang_code} sound statistics:")
    print(f"  - Base language (en-US): {base_sound_count} sounds")
    print(f"  - User language: {user_sound_count} sounds")
    print(f"  - Input-fix sounds: {input_fix_sound_count} sounds")
    print(f"  - Common sounds: {common_sound_count} sounds")
    if sound_fallback_count > 0:
        print(f"  - Sound fallback to en-US: {sound_fallback_count} sounds")
    
    # 生成语言特定音效常量
    for file in sorted(all_sound_files):
        base_name = os.path.splitext(file)[0]
        # 优先使用当前语言的音效，如果不存在则回退到 en-US
        if file in current_sounds or file in input_fix_sounds:
            sound_lang = lang_code.replace('-', '_').lower()
        else:
            sound_lang = 'en_us'
            
        sounds.append(f'''
        extern const char ogg_{base_name}_start[] asm("_binary_{base_name}_ogg_start");
        extern const char ogg_{base_name}_end[] asm("_binary_{base_name}_ogg_end");
        static const std::string_view OGG_{base_name.upper()} {{
        static_cast<const char*>(ogg_{base_name}_start),
        static_cast<size_t>(ogg_{base_name}_end - ogg_{base_name}_start)
        }};''')
    
    # 生成公共音效常量
    for file in sorted(common_sounds_files):
        base_name = os.path.splitext(file)[0]
        sounds.append(f'''
        extern const char ogg_{base_name}_start[] asm("_binary_{base_name}_ogg_start");
        extern const char ogg_{base_name}_end[] asm("_binary_{base_name}_ogg_end");
        static const std::string_view OGG_{base_name.upper()} {{
        static_cast<const char*>(ogg_{base_name}_start),
        static_cast<size_t>(ogg_{base_name}_end - ogg_{base_name}_start)
        }};''')

    # 填充模板
    content = HEADER_TEMPLATE.format(
        lang_code=lang_code,
        lang_code_for_font=lang_code.replace('-', '_').lower(),
        strings="\n".join(sorted(strings)),
        sounds="\n".join(sorted(sounds))
    )

    # 写入文件
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write(content)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Generate language configuration header file with en-US fallback")
    parser.add_argument("--input", required=True, help="language.json file path")
    parser.add_argument("--input_fix", required=False, help="Additional assets directory with sound files")  # 改为下划线
    parser.add_argument("--output", required=True, help="Output header file path")
    args = parser.parse_args()

    try:
        generate_header(args.input, args.input_fix, args.output)  # 对应修改
        print(f"Successfully generated language config file: {args.output}")
    except Exception as e:
        print(f"Error: {e}")
        exit(1)