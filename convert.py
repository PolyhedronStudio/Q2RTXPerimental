#!/usr/bin/env python3
"""
C to C++ Conversion Script

This script performs basic transformations to help convert C code to C++.
It handles:
- Replacing C-style casts with C++ casts
- Converting malloc/free to new/delete
- Adding const to appropriate function parameters
- Converting C-style comments to C++ style
- Adding extern "C" for external C function declarations
- Converting C headers to C++ headers
- Converting NULL to nullptr

Usage: python c_to_cpp_converter.py input_file.c output_file.cpp
"""

import re
import sys
import os

def replace_c_casts(line):
    # Replace C-style casts with C++ static_cast
    # This is a simplified approach and may need manual review
    pattern = r'\(([A-Za-z0-9_]+\s*\**)\)\s*([^,);]+)'
    replacement = r'static_cast<\1>(\2)'
    return re.sub(pattern, replacement, line)

def replace_malloc_free(line):
    # Replace malloc with new
    malloc_pattern = r'([A-Za-z0-9_]+\s*\*)\s*=\s*\(\s*[A-Za-z0-9_]+\s*\*\s*\)\s*malloc\s*\(\s*sizeof\s*\(\s*([A-Za-z0-9_]+)\s*\)\s*\*\s*([^)]+)\s*\)'
    replacement = r'\1 = new \2[\3]'
    line = re.sub(malloc_pattern, replacement, line)
    
    # Replace calloc with new and initialization
    calloc_pattern = r'([A-Za-z0-9_]+\s*\*)\s*=\s*\(\s*[A-Za-z0-9_]+\s*\*\s*\)\s*calloc\s*\(\s*([^,]+)\s*,\s*sizeof\s*\(\s*([A-Za-z0-9_]+)\s*\)\s*\)'
    replacement = r'\1 = new \3[\2](); // Zero-initialized'
    line = re.sub(calloc_pattern, replacement, line)
    
    # Replace free with delete[]
    free_pattern = r'free\s*\(\s*([^)]+)\s*\)'
    replacement = r'delete[] \1'
    return re.sub(free_pattern, replacement, line)

def add_const_to_params(line):
    # Add const to string parameters
    pattern = r'(char\s*\*\s*)([a-zA-Z_][a-zA-Z0-9_]*)'
    replacement = r'const \1\2'
    return re.sub(pattern, replacement, line)

def convert_comments(line):
    # Convert C-style comments to C++ style
    if '/*' in line and '*/' in line:
        pattern = r'/\*(.*?)\*/'
        replacement = r'// \1'
        return re.sub(pattern, replacement, line)
    return line

def replace_null_with_nullptr(line):
    # Replace NULL with nullptr
    pattern = r'\bNULL\b'
    replacement = 'nullptr'
    return re.sub(pattern, replacement, line)

def convert_headers(line):
    # Convert C headers to C++ headers
    c_headers = {
        'stdio.h': 'cstdio',
        'stdlib.h': 'cstdlib',
        'string.h': 'cstring',
        'math.h': 'cmath',
        'assert.h': 'cassert',
        'time.h': 'ctime',
        'stdint.h': 'cstdint',
        'stdbool.h': 'cstdbool'
    }
    
    for c_header, cpp_header in c_headers.items():
        pattern = f'#include <{c_header}>'
        replacement = f'#include <{cpp_header}>'
        line = line.replace(pattern, replacement)
    
    return line

def add_extern_c(content):
    # Add extern "C" for external C function declarations
    # This is a simplified approach and needs manual review
    function_pattern = r'(\w+\s+\w+\s*\([^)]*\)\s*;)'
    
    def add_extern(match):
        return f'extern "C" {match.group(1)}'
    
    return re.sub(function_pattern, add_extern, content)

def convert_file(input_file, output_file):
    # Read the input file
    with open(input_file, 'r') as file:
        content = file.read()
    
    # Apply the transformations line by line
    lines = content.split('\n')
    converted_lines = []
    
    for line in lines:
        line = replace_c_casts(line)
        line = replace_malloc_free(line)
        line = add_const_to_params(line)
        line = convert_comments(line)
        line = replace_null_with_nullptr(line)
        line = convert_headers(line)
        converted_lines.append(line)
    
    converted_content = '\n'.join(converted_lines)
    # Add extern "C" for function declarations (if needed)
    # converted_content = add_extern_c(converted_content)
    
    # Add .cpp file header
    cpp_header = """// Converted from C to C++ using automatic conversion script
// Manual review and adjustments may be needed

#include <memory>
#include <vector>
#include <string>
#include <algorithm>

"""
    
    # Write the converted content to the output file
    with open(output_file, 'w') as file:
        file.write(cpp_header + converted_content)
    
    print(f"Conversion complete. Output written to {output_file}")

def process_directory(input_dir, output_dir):
    """Process all .c and .h files in a directory."""
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)
    
    for filename in os.listdir(input_dir):
        input_path = os.path.join(input_dir, filename)
        
        if os.path.isfile(input_path) and (filename.endswith('.c') or filename.endswith('.h')):
            # Determine output filename (change .c to .cpp, keep .h as .h)
            output_filename = filename.replace('.c', '.cpp') if filename.endswith('.c') else filename
            output_path = os.path.join(output_dir, output_filename)
            
            print(f"Converting {input_path} to {output_path}")
            convert_file(input_path, output_path)

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python c_to_cpp_converter.py input_file.c output_file.cpp")
        print("   or: python c_to_cpp_converter.py input_directory output_directory")
        sys.exit(1)
    
    input_path = sys.argv[1]
    output_path = sys.argv[2]
    
    if os.path.isdir(input_path):
        process_directory(input_path, output_path)
    else:
        convert_file(input_path, output_path)
