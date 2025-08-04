#!/usr/bin/env python3
import struct
import sys
import datetime
import os

# --- Configuration ---
DEFAULT_BINARY_LOG_PATH = "/home/ubuntu/bridge/build/bridge_binary.log"
DEFAULT_OUTPUT_TXT_PATH = "/home/ubuntu/bridge/bridge_decoded.txt"

# Define CppLogging Level enum mapping
# From: /home/ubuntu/external_libs/CppLogging/include/logging/level.h
LOG_LEVELS = {
    0x00: "NONE", 0x1F: "FATAL", 0x3F: "ERROR",
    0x7F: "WARN", 0x9F: "INFO", 0xBF: "DEBUG", 0xFF: "ALL"
}

# ArgumentType enum values from CppLogging, deduced from output.
# Using struct format strings: '<' for little-endian.
ARG_TYPES = {
    0x01: ('<?', 1),    # bool (1 byte)
    0x02: ('c', 1),     # char (1 byte)
    0x04: ('<b', 1),    # int8_t (1 byte)
    0x05: ('<B', 1),    # uint8_t (1 byte)
    0x06: ('<h', 2),    # int16_t (2 bytes)
    0x07: ('<H', 2),    # uint16_t (2 bytes)
    0x08: ('<i', 4),    # int32_t (4 bytes)
    0x09: ('<I', 4),    # uint32_t (4 bytes)
    0x0A: ('<q', 8),    # int64_t (8 bytes)
    0x0B: ('<Q', 8),    # uint64_t (8 bytes)
    0x0C: ('<f', 4),    # float (4 bytes)
    0x0D: ('<d', 8),    # double (8 bytes)
    0x0E: 'string',     # Special case for variable-length string
}

def format_timestamp_ns(timestamp_ns):
    """Formats a nanosecond timestamp into YYYY-MM-DDTHH:MM:SS.mmm.uuu.nnnZ"""
    if timestamp_ns < 0:
        return "InvalidTimestamp"
    seconds = timestamp_ns // 1_000_000_000
    nanoseconds_remainder = timestamp_ns % 1_000_000_000
    try:
        dt = datetime.datetime.fromtimestamp(seconds, tz=datetime.timezone.utc)
        return dt.strftime('%Y-%m-%dT%H:%M:%S') + f".{nanoseconds_remainder:09d}Z"
    except (OSError, ValueError):
        return f"TimestampError({timestamp_ns})"

def _parse_argument_buffer(buffer):
    """Parses the binary argument buffer and returns a list of Python objects."""
    args = []
    offset = 0
    while offset < len(buffer):
        try:
            arg_type_val = buffer[offset]
            offset += 1

            if arg_type_val in ARG_TYPES:
                handler = ARG_TYPES[arg_type_val]
                if handler == 'string':
                    # String: 4-byte length + N bytes of UTF-8 data
                    if offset + 4 > len(buffer): break
                    str_len = struct.unpack_from('<I', buffer, offset)[0]
                    offset += 4
                    if offset + str_len > len(buffer): break
                    value = buffer[offset:offset+str_len].decode('utf-8', errors='replace')
                    args.append(value)
                    offset += str_len
                else:
                    # Fixed-size types
                    fmt_char, size = handler
                    if offset + size > len(buffer): break
                    value = struct.unpack_from(fmt_char, buffer, offset)[0]
                    args.append(value)
                    offset += size
            else:
                # Unknown argument type
                args.append(f"{{UNKNOWN_ARG_TYPE:0x{arg_type_val:02X}}}")
                break # Stop parsing this record's args
        except Exception:
            args.append("{PARSING_ERROR}")
            break
    return args

def parse_record(data_block):
    """Parses a single log record data block."""
    offset = 0

    timestamp_ns = struct.unpack_from('<Q', data_block, offset)[0]; offset += 8
    thread_id = struct.unpack_from('<Q', data_block, offset)[0]; offset += 8
    level_val = struct.unpack_from('<B', data_block, offset)[0]; offset += 1
    level_str = LOG_LEVELS.get(level_val, f"UNKNOWN_LVL(0x{level_val:02X})")
    
    logger_name_size = struct.unpack_from('<B', data_block, offset)[0]; offset += 1
    logger_name = data_block[offset : offset + logger_name_size].decode('utf-8', errors='replace'); offset += logger_name_size
    
    message_size = struct.unpack_from('<H', data_block, offset)[0]; offset += 2
    raw_message_str = data_block[offset : offset + message_size].decode('utf-8', errors='replace'); offset += message_size

    effective_message = raw_message_str

    if offset + 4 > len(data_block):
        return f"Warning: Truncated record. RawMsg: '{raw_message_str}'"

    arg_data_actual_len = struct.unpack_from('<I', data_block, offset)[0]
    offset += 4

    if arg_data_actual_len > 0:
        if offset + arg_data_actual_len > len(data_block):
            effective_message += " (TruncatedArgs)"
        else:
            arg_buffer = data_block[offset : offset + arg_data_actual_len]
            try:
                parsed_args = _parse_argument_buffer(arg_buffer)
                # Use python's format() to substitute arguments
                effective_message = raw_message_str.format(*parsed_args)
            except (IndexError, ValueError) as e:
                # Formatting failed (e.g., mismatched {} count vs args)
                args_repr = ", ".join(map(str, parsed_args))
                effective_message = f"{raw_message_str} [FORMAT_ERROR: {e} | ARGS: {args_repr}]"

    formatted_timestamp = format_timestamp_ns(timestamp_ns)
    # Use only the lower 32 bits of the thread ID for cleaner output, matching the C++ tool
    return f"{formatted_timestamp} [0x{thread_id & 0xFFFFFFFF:X}] {level_str:<5} {logger_name} - {effective_message}"

def decode_log_file(binary_filepath, output_filepath):
    """Decodes records from a CppLogging binary log file and writes to text file."""
    
    # Check if binary log file exists
    if not os.path.exists(binary_filepath):
        print(f"Error: Binary log file not found at '{binary_filepath}'")
        print(f"Make sure your application has run and generated the log file.")
        return False
    
    # Get file size for progress indication
    file_size = os.path.getsize(binary_filepath)
    if file_size == 0:
        print(f"Warning: Binary log file '{binary_filepath}' is empty.")
        return False
    
    records_processed = 0
    warnings_count = 0
    errors_count = 0
    
    try:
        with open(binary_filepath, 'rb') as infile, open(output_filepath, 'w', encoding='utf-8') as outfile:
            # Write header information
            outfile.write(f"# CppLogging Binary Log Decoder Output\n")
            outfile.write(f"# Source: {binary_filepath}\n")
            outfile.write(f"# Decoded at: {datetime.datetime.now().isoformat()}\n")
            outfile.write(f"# File size: {file_size} bytes\n")
            outfile.write("# Format: TIMESTAMP [THREAD_ID] LEVEL LOGGER - MESSAGE\n")
            outfile.write("#" + "="*80 + "\n\n")
            
            while True:
                size_bytes = infile.read(4)
                if not size_bytes: 
                    break
                if len(size_bytes) < 4:
                    outfile.write(f"ERROR: Incomplete record size field at end of file.\n")
                    errors_count += 1
                    break
                
                data_block_size = struct.unpack('<I', size_bytes)[0]
                
                if data_block_size == 0: 
                    outfile.write(f"WARNING: Encountered zero-size data block. Skipping.\n")
                    warnings_count += 1
                    continue

                data_block = infile.read(data_block_size)
                if len(data_block) < data_block_size:
                    outfile.write(f"ERROR: Incomplete data block. Expected {data_block_size}, got {len(data_block)}.\n")
                    errors_count += 1
                    break
                
                try:
                    formatted_record = parse_record(data_block)
                    outfile.write(formatted_record + "\n")
                    records_processed += 1
                    
                    # Progress indicator for large files
                    if records_processed % 1000 == 0:
                        print(f"Processed {records_processed} records...")
                        
                except struct.error as e:
                    outfile.write(f"ERROR: Struct parsing error: {e}. Data block size: {data_block_size}\n")
                    errors_count += 1
                except UnicodeDecodeError as e:
                    outfile.write(f"ERROR: Unicode decoding error: {e}. Data block size: {data_block_size}\n")
                    errors_count += 1
                except Exception as e:
                    outfile.write(f"ERROR: Unexpected error: {e}. Data block size: {data_block_size}\n")
                    errors_count += 1

    except FileNotFoundError:
        print(f"Error: File not found at '{binary_filepath}'")
        return False
    except PermissionError:
        print(f"Error: Permission denied accessing '{binary_filepath}' or '{output_filepath}'")
        return False
    except Exception as e:
        print(f"Error: An unexpected error occurred: {e}")
        return False

    # Summary
    print(f"Decoding completed successfully!")
    print(f"  Records processed: {records_processed}")
    print(f"  Warnings: {warnings_count}")
    print(f"  Errors: {errors_count}")
    print(f"  Output written to: {output_filepath}")
    print(f"  Output file size: {os.path.getsize(output_filepath)} bytes")
    
    return True

def main():
    """Main function with improved argument handling."""
    
    # Determine input and output paths
    if len(sys.argv) == 1:
        # No arguments - use defaults
        binary_path = DEFAULT_BINARY_LOG_PATH
        output_path = DEFAULT_OUTPUT_TXT_PATH
        print(f"Using default paths:")
        print(f"  Binary log: {binary_path}")
        print(f"  Output txt: {output_path}")
        
    elif len(sys.argv) == 2:
        # One argument - binary file path provided, use default output
        binary_path = sys.argv[1]
        output_path = DEFAULT_OUTPUT_TXT_PATH
        print(f"Using provided binary path: {binary_path}")
        print(f"Using default output path: {output_path}")
        
    elif len(sys.argv) == 3:
        # Two arguments - both paths provided
        binary_path = sys.argv[1]
        output_path = sys.argv[2]
        print(f"Using provided paths:")
        print(f"  Binary log: {binary_path}")
        print(f"  Output txt: {output_path}")
        
    else:
        print("Usage:")
        print(f"  {sys.argv[0]}                              # Use default paths")
        print(f"  {sys.argv[0]} <binary_log_file>            # Specify binary file, use default output")
        print(f"  {sys.argv[0]} <binary_log_file> <output.txt> # Specify both files")
        print()
        print(f"Default binary log path: {DEFAULT_BINARY_LOG_PATH}")
        print(f"Default output txt path: {DEFAULT_OUTPUT_TXT_PATH}")
        sys.exit(1)
    
    # Validate and decode
    if not decode_log_file(binary_path, output_path):
        sys.exit(1)

if __name__ == "__main__":
    main()