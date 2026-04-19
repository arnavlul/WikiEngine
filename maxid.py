import json
import sys

# Change this path if needed
DOC_INFO_PATH = "data_files\\doc_info.jsonl"

def find_max_id(filepath):
    print(f"Scanning {filepath} for maximum ID...")
    
    max_id = -1
    count = 0
    
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            for line in f:
                if not line.strip(): continue
                try:
                    data = json.loads(line)
                    current_id = data.get('id', -1)
                    
                    if current_id > max_id:
                        max_id = current_id
                        
                    count += 1
                    if count % 1000000 == 0:
                        print(f"Scanned {count} lines... Max so far: {max_id}", end='\r')
                        
                except json.JSONDecodeError:
                    continue
                    
    except FileNotFoundError:
        print(f"Error: Could not find file at {filepath}")
        return

    print(f"\n\n--- Scan Complete ---")
    print(f"Total Documents: {count}")
    print(f"Highest ID Found: {max_id}")

if __name__ == "__main__":
    find_max_id(DOC_INFO_PATH)