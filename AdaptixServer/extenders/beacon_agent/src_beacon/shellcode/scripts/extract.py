#!/usr/bin/env python3
# -*- coding:utf-8 -*-
"""
AdaptixC2 Shellcode Extractor
- Extract .text section from PE file
- Search and replace STUB_SIZE_MARKER with actual stub size
- Pad to aligned size
"""

import pefile
import argparse
import sys
import struct

# Marker definitions (must be less than 0x7FFFFFFF to avoid x64 sign extension issues)
MARKER_X64 = 0x13371337
MARKER_X86 = 0x13381338

# Alignment size (256 bytes)
ALIGNMENT = 0x100

def detect_arch(pe):
    """Detect PE file architecture"""
    machine = pe.FILE_HEADER.Machine
    if machine == 0x8664:  # IMAGE_FILE_MACHINE_AMD64
        return 'x64'
    elif machine == 0x014c:  # IMAGE_FILE_MACHINE_I386
        return 'x86'
    else:
        return 'unknown'

def extract_and_patch(input_file, output_file, verbose=False):
    """Extract shellcode and patch STUB_SIZE_MARKER"""
    try:
        pe = pefile.PE(input_file)

        # Detect architecture
        arch = detect_arch(pe)
        if verbose:
            print(f'[+] Detected architecture: {arch}')

        # Get first section (.text)
        section_data = pe.sections[0].get_data()

        # Find ENDOFCODE marker
        end_marker = section_data.find(b'ENDOFCODE')
        if end_marker == -1:
            print('[!] Error: ENDOFCODE marker not found')
            sys.exit(1)

        # Extract data before ENDOFCODE
        shellcode = bytearray(section_data[:end_marker])

        if verbose:
            print(f'[+] Extracted shellcode size: 0x{len(shellcode):X} ({len(shellcode)} bytes)')

        # Select marker based on architecture
        if arch == 'x64':
            marker_bytes = struct.pack('<I', MARKER_X64)
            marker_name = 'MARKER_X64 (0x13371337)'
        else:
            marker_bytes = struct.pack('<I', MARKER_X86)
            marker_name = 'MARKER_X86 (0x13381338)'

        # Search for marker
        marker_pos = shellcode.find(marker_bytes)
        if marker_pos == -1:
            print(f'[!] Error: {marker_name} not found in shellcode')
            sys.exit(1)

        if verbose:
            print(f'[+] Found {marker_name} at offset: 0x{marker_pos:X}')

        # Calculate aligned stub size
        raw_size = len(shellcode)
        aligned_size = (raw_size + ALIGNMENT - 1) & ~(ALIGNMENT - 1)

        if verbose:
            print(f'[+] Raw size: 0x{raw_size:X}, Aligned size: 0x{aligned_size:X}')

        # Replace marker with actual aligned size
        struct.pack_into('<I', shellcode, marker_pos, aligned_size)

        if verbose:
            print(f'[+] Patched STUB_SIZE_MARKER with: 0x{aligned_size:X}')

        # Pad to aligned size
        padding_size = aligned_size - raw_size
        padded_shellcode = shellcode + b'\x00' * padding_size

        if verbose:
            print(f'[+] Added {padding_size} bytes of padding')

        # Write to output file
        with open(output_file, 'wb') as f:
            f.write(padded_shellcode)

        if verbose:
            print(f'[+] Written {len(padded_shellcode)} bytes to: {output_file}')

        return True

    except Exception as e:
        print(f'[!] Error: {e}')
        sys.exit(1)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='AdaptixC2 Shellcode Extractor')
    parser.add_argument('-f', required=True, help='Path to the source executable', type=str)
    parser.add_argument('-o', required=True, help='Path to store the output raw binary', type=str)
    parser.add_argument('-v', '--verbose', action='store_true', help='Verbose output')

    args = parser.parse_args()

    extract_and_patch(args.f, args.o, args.verbose)