#!/usr/bin/env python3
# filepath: /home/ubuntu/ros_ws/BinLogDecoder.py
import struct
import sys
import datetime
import os

# --- Configuration ---
DEFAULT_BINARY_LOG_PATH = "/home/ubuntu/ros_ws/mec_binary.log"
DEFAULT_OUTPUT_TXT_PATH = "/home/ubuntu/ros_ws/decoded_log.txt"

# Define CppLogging Level enum mapping
# From: /home/ubuntu/external_libs/CppLogging/include/logging/level.h
LOG_LEVELS = {
    0x00: "NONE",
    0x1F: "FATAL",
    0x3F: "ERROR",
    0x7F: "WARN",
    0x9F: "INFO",
    0xBF: "DEBUG",
    0xFF: "ALL"
}

# ArgumentType enum value for string
# In this binary format, string arguments are using type code 0x0E (not 0x0D as initially assumed)
ARG_STRING_TYPE = 0x0E

def format_timestamp_ns(timestamp_ns):
    """Formats a nanosecond timestamp into YYYY-MM-DDTHH:MM:SS.mmm.uuu.nnnZ"""
    if timestamp_ns < 0:
        return "InvalidTimestamp"
    
    seconds = timestamp_ns // 1_000_000_000
    nanoseconds_remainder = timestamp_ns % 1_000_000_000
    
    milliseconds = nanoseconds_remainder // 1_000_000
    microseconds_in_ms_remainder = (nanoseconds_remainder % 1_000_000) // 1_000
    nanoseconds_in_us_remainder = nanoseconds_remainder % 1_000
    
    try:
        dt_object = datetime.datetime.fromtimestamp(seconds, tz=datetime.timezone.utc)
        return dt_object.strftime('%Y-%m-%dT%H:%M:%S') + \
               f".{milliseconds:03d}.{microseconds_in_ms_remainder:03d}.{nanoseconds_in_us_remainder:03d}Z"
    except ValueError:
        return f"TimestampError({timestamp_ns})"

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

    # Check for argument buffer size field
    if offset + 4 > len(data_block):
        return f"Warning: Truncated record before arg_buffer_size. RawMsg: '{raw_message_str}'"

    arg_data_actual_len = struct.unpack_from('<I', data_block, offset)[0]
    offset += 4

    # If the raw message is "{}" and there's argument data, try to parse the first argument as a string
    if raw_message_str == "{}" and arg_data_actual_len > 0:
        if offset + arg_data_actual_len > len(data_block):
            return f"Warning: Arg data length {arg_data_actual_len} exceeds remaining data_block {len(data_block) - offset}. RawMsg: '{raw_message_str}'"
        else:
            arg_buffer = data_block[offset : offset + arg_data_actual_len]
            
            # Try to parse the first argument if it's a string
            # Min size for string arg: 1 (type) + 4 (len_field) = 5 bytes
            if len(arg_buffer) >= 5:
                arg_ptr = 0 # Pointer within arg_buffer
                
                arg_type_val = struct.unpack_from('<B', arg_buffer, arg_ptr)[0]; arg_ptr += 1
                
                if arg_type_val == ARG_STRING_TYPE:
                    if arg_ptr + 4 <= len(arg_buffer): # Check for string length field
                        str_len_in_arg = struct.unpack_from('<I', arg_buffer, arg_ptr)[0]; arg_ptr += 4
                        
                        if arg_ptr + str_len_in_arg <= len(arg_buffer): # Check for string data
                            actual_str_data = arg_buffer[arg_ptr : arg_ptr + str_len_in_arg]
                            try:
                                decoded_arg_str = actual_str_data.decode('utf-8', errors='replace')
                                effective_message = decoded_arg_str # Replace "{}"
                            except Exception as e:
                                return f"Warning: Failed to decode string arg: {e}"
                        # else: String data length exceeds arg_buffer, effective_message remains "{}"
                    # else: String length field exceeds arg_buffer, effective_message remains "{}"
                # else: First argument is not a string, effective_message remains "{}"
            # else: Argument buffer too small for a typed string, effective_message remains "{}"
    
    offset += arg_data_actual_len # Advance offset past the argument data

    if offset != len(data_block):
        return f"Warning: Offset mismatch after parsing. Offset={offset}, DataBlockLen={len(data_block)}. EffectiveMsg: '{effective_message}'"

    formatted_timestamp = format_timestamp_ns(timestamp_ns)
    return f"{formatted_timestamp} [0x{thread_id:X}] {level_str:<5} {logger_name} - {effective_message}"

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