# Encode the input string to C array format
import sys
import os
import argparse
import re
import json

def encode_string_to_c_array(input_string):
    # Remove non-printable characters
    input_string = re.sub(r'[^ -~]', '', input_string)

    # Convert to C array format
    c_array = ', '.join(f'0x{ord(c):02X}' for c in input_string)

    # Ensure it is null-terminated
    if input_string:
        c_array += ', 0x00'
    else:
        c_array = '0x00'

    return f'{{ {c_array} }}'

def main():
    parser = argparse.ArgumentParser(description='Encode a string to C array format.')
    parser.add_argument('input_string', type=str, help='The input string to encode.')
    parser.add_argument('--output', type=str, default='output.c', help='Output file name.')
    args = parser.parse_args()

    # Encode the input string
    c_array = encode_string_to_c_array(args.input_string)

    # Write to output file
    with open(args.output, 'w') as f:
        f.write(c_array)
        f.write('\n')
        f.write(f'// Length: {len(args.input_string)}\n')
        f.write(f'// Input: {args.input_string}\n')
    print(f'Encoded string written to {args.output}')
if __name__ == '__main__':
    main()

# Example usage:
# python encode_str_to_c.py "Hello, World!" --output hello_world.c
# The output file will contain the encoded C array and some metadata.
# Note: This script removes non-printable characters from the input string before encoding.
# This is to ensure that the C array only contains valid ASCII characters.
# The script also includes a command-line interface for easy usage.