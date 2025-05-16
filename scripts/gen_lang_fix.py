#!/usr/bin/env python3
import argparse
import json
import os
import re
import sys
import gen_lang


HEADER_TEMPLATE = """// Auto-generated language config
#pragma once

#include <string_view>

#ifndef {lang_code_for_font}
    #define {lang_code_for_font}  // 預設語言
#endif

namespace Lang {{
    // 语言元数据
    constexpr const char* CODE = "{lang_code}";

    // 字符串资源
    namespace Strings {{
{strings}
    }}

    // 音效资源
    namespace Sounds {{
{sounds}
    }}
}}
"""


def parse_config(file):
    with open(file, 'r', encoding='utf-8') as f:
        code = f.read()
    
    lang_code = 'zh-CN'
    strings_dict = {}
    sounds_array = []
    
    # 解析字符串资源
    code_pattern = re.compile(r'// 语言元数据\n\s+constexpr const char\* CODE = "([^"]+)"')
    code_match = code_pattern.search(code)
    lang_code = code_match.group(1)
            
    # 解析字符串资源
    strings_pattern = re.compile(r'namespace Strings\s*\{([^}]*)\}')
    strings_block = strings_pattern.search(code)
    if strings_block:
        strings_content = strings_block.group(1)
        for line in strings_content.splitlines():
            match = re.match(r'\s*constexpr const char\* (\w+) = "([^"]+)"', line)
            if match:
                key, value = match.groups()
                strings_dict[key] = value

    # 解析音效资源
    sounds_pattern = re.compile(r'static const std::string_view P3_(\w+) {')
    sounds_matches = sounds_pattern.findall(code)
    sounds_array = [name.lower() for name in sounds_matches]  # 转换为小写并创建集合

    return lang_code, strings_dict, sounds_array


def gen_config(file, lang_code, strings_dict, sounds_array):
    strings = []
    sounds = []
    for key, value in strings_dict.items():
        value = value.replace('"', '\\"')
        strings.append(f'        constexpr const char* {key.upper()} = "{value}";')

    # 生成音效常量
    for base_name in sounds_array:
        sounds.append(f'''
        extern const char p3_{base_name}_start[] asm("_binary_{base_name}_p3_start");
        extern const char p3_{base_name}_end[] asm("_binary_{base_name}_p3_end");
        static const std::string_view P3_{base_name.upper()} {{
        static_cast<const char*>(p3_{base_name}_start),
        static_cast<size_t>(p3_{base_name}_end - p3_{base_name}_start)
        }};''')

    # 填充模板
    content = HEADER_TEMPLATE.format(
        lang_code=lang_code,
        lang_code_for_font=lang_code.replace('-', '_').lower(),
        strings="\n".join(sorted(strings)),
        sounds="\n".join(sorted(sounds))
    )
    
    # 写入文件
    os.makedirs(os.path.dirname(file), exist_ok=True)
    with open(file, 'w', encoding='utf-8') as f:
        f.write(content)

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True, help="输入JSON文件路径")
    parser.add_argument("--input_fix", required=True, help="输入JSON文件路径")
    parser.add_argument("--output", required=True, help="输出头文件路径")
    args = parser.parse_args()
    
    gen_lang.generate_header(args.input, args.output)
    
    if not os.path.exists(args.output):
        sys.exit(-1)
    
    lang_code, strings_dict, sounds_array = parse_config(args.output)
    
    if os.path.exists(args.input_fix):
        with open(args.input_fix, 'r', encoding='utf-8') as f:
            data = json.load(f)

            # 验证数据结构
            if 'strings' not in data:
                raise ValueError("Invalid JSON structure")
            
            for key, value in data['strings'].items():
                strings_dict[key] = value
    if os.path.exists(os.path.dirname(args.input_fix)):
        for file in os.listdir(os.path.dirname(args.input_fix)):
            if file.endswith('.p3'):
                base_name = os.path.splitext(file)[0]
                if base_name not in sounds_array:
                    sounds_array.append(base_name)
        
        if os.path.exists(os.path.join(os.path.dirname(args.input_fix), '..', 'common')):
            for file in os.listdir(os.path.join(os.path.dirname(args.input_fix), '..', 'common')):
                if file.endswith('.p3'):
                    base_name = os.path.splitext(file)[0]
                    if base_name not in sounds_array:
                        sounds_array.append(base_name)
    
    gen_config(args.output, lang_code, strings_dict, sounds_array)
    
    
