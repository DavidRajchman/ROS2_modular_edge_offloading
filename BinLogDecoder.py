import struct
import sys
import datetime

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
        print(f"Warning: Truncated record before arg_buffer_size. RawMsg: '{raw_message_str}'", file=sys.stderr)
        formatted_timestamp = format_timestamp_ns(timestamp_ns)
        return f"{formatted_timestamp} [0x{thread_id:X}] {level_str:<5} {logger_name} - {effective_message} (TruncatedArgs)"

    arg_data_actual_len = struct.unpack_from('<I', data_block, offset)[0]
    offset += 4

    # If the raw message is "{}" and there's argument data, try to parse the first argument as a string
    if raw_message_str == "{}" and arg_data_actual_len > 0:
        if offset + arg_data_actual_len > len(data_block):
            print(f"Warning: Arg data length {arg_data_actual_len} exceeds remaining data_block {len(data_block) - offset}. RawMsg: '{raw_message_str}'", file=sys.stderr)
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
                                print(f"Warning: Failed to decode string arg: {e}", file=sys.stderr)
                                # effective_message remains "{}"
                        # else: String data length exceeds arg_buffer, effective_message remains "{}"
                    # else: String length field exceeds arg_buffer, effective_message remains "{}"
                # else: First argument is not a string, effective_message remains "{}"
            # else: Argument buffer too small for a typed string, effective_message remains "{}"
    
    offset += arg_data_actual_len # Advance offset past the argument data

    if offset != len(data_block):
        print(f"Warning: Offset mismatch after parsing. Offset={offset}, DataBlockLen={len(data_block)}. EffectiveMsg: '{effective_message}'", file=sys.stderr)

    formatted_timestamp = format_timestamp_ns(timestamp_ns)
    return f"{formatted_timestamp} [0x{thread_id:X}] {level_str:<5} {logger_name} - {effective_message}"

def decode_log_file(filepath):
    """Decodes and prints records from a CppLogging binary log file."""
    try:
        with open(filepath, 'rb') as f:
            while True:
                size_bytes = f.read(4)
                if not size_bytes: break
                if len(size_bytes) < 4:
                    print(f"Error: Incomplete record size field at end of file.", file=sys.stderr); break
                
                data_block_size = struct.unpack('<I', size_bytes)[0]
                
                if data_block_size == 0: 
                    print(f"Warning: Encountered zero-size data block. Skipping.", file=sys.stderr); continue

                data_block = f.read(data_block_size)
                if len(data_block) < data_block_size:
                    print(f"Error: Incomplete data block. Expected {data_block_size}, got {len(data_block)}.", file=sys.stderr); break
                
                try:
                    formatted_record = parse_record(data_block)
                    print(formatted_record)
                except struct.error as e:
                    print(f"Error parsing record (struct error): {e}. Data block size: {data_block_size}", file=sys.stderr)
                except UnicodeDecodeError as e:
                    print(f"Error parsing record (unicode error): {e}. Data block size: {data_block_size}", file=sys.stderr)
                except Exception as e:
                    print(f"An unexpected error occurred while parsing a record: {e}", file=sys.stderr)

    except FileNotFoundError:
        print(f"Error: File not found at '{filepath}'", file=sys.stderr)
    except Exception as e:
        print(f"An error occurred: {e}", file=sys.stderr)

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python BinLogDecoder.py <path_to_binary_log_file>")
        sys.exit(1)
    
    log_file_path = sys.argv[1]
    decode_log_file(log_file_path)