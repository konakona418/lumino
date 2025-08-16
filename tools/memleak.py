import json
import sys

def detect_memory_leaks(log_file_path):
    allocated_blocks = {}
    total_allocations = 0
    total_frees = 0

    try:
        with open(log_file_path, 'r') as f:
            for line_number, line in enumerate(f, 1):
                line = line.strip()
                if not line:
                    continue

                try:
                    data = json.loads(line)
                    action = data.get('action')
                    addr = data.get('addr')

                    if not action or not addr:
                        print(f"Warning: Malformed line {line_number}, missing 'action' or 'addr': {line}", file=sys.stderr)
                        continue
                    
                    addr = addr.lstrip('0x').lower()

                    if action in ['alloc', 'calloc', 'alloc_array']:
                        total_allocations += 1
                        if addr in allocated_blocks:
                            print(f"Warning: Duplicate allocation address {addr} on line {line_number}.", file=sys.stderr)
                        
                        allocated_blocks[addr] = {
                            'type': data.get('type'),
                            'size': data.get('size') if action != 'alloc_array' else data.get('element_size'),
                            'count': data.get('count', 1),
                            'line': line_number
                        }
                    elif action == 'free':
                        total_frees += 1
                        if addr in allocated_blocks:
                            del allocated_blocks[addr]
                        else:
                            print(f"Warning: Attempted to free unallocated or already freed address {addr} on line {line_number}.", file=sys.stderr)
                    else:
                        print(f"Warning: Unsupported action type '{action}' on line {line_number}.", file=sys.stderr)

                except json.JSONDecodeError:
                    print(f"Error: Line {line_number} is not valid JSON: {line}", file=sys.stderr)
                except Exception as e:
                    print(f"Error: An exception occurred on line {line_number}: {e}", file=sys.stderr)

    except FileNotFoundError:
        print(f"Error: File '{log_file_path}' not found.", file=sys.stderr)
        return 0, {}

    total_leaked_bytes = 0
    leaked_blocks_info = {}
    if allocated_blocks:
        print("\n--- Memory leak detected! ---\n")
        print(f"Total allocations: {total_allocations}, Total frees: {total_frees}")
        print(f"Found {len(allocated_blocks)} leaked memory blocks.")
        for addr, info in allocated_blocks.items():
            if info.get('size') and info.get('count'):
                leaked_bytes = info['size'] * info['count']
                total_leaked_bytes += leaked_bytes
                
                print(f"Address: {addr}, Type: {info.get('type', 'Unknown')}, "
                      f"Size: {leaked_bytes} bytes ({info['size']} * {info['count']})")
                
                leaked_blocks_info[addr] = info
        
        print(f"\nTotal leaked bytes: {total_leaked_bytes} bytes.")
    else:
        print("\n--- No memory leaks detected. All allocated memory was properly freed. ---\n")

    return total_leaked_bytes, leaked_blocks_info

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python memleak.py <log_file_path>")
        sys.exit(1)
    
    log_file = sys.argv[1]
    detect_memory_leaks(log_file)