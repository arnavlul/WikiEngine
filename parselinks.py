import gzip
import json
import re
import multiprocessing
import time
import os
import sys

# --- Configuration ---
PAGELINKS_FILE = "enwiki-20251101-pagelinks.sql.gz"
LINKTARGET_FILE = "enwiki-20251101-linktarget.sql.gz"
# UPDATE THIS PATH IF NEEDED
DOC_INFO_FILE = r"C:\Users\Arnav\Desktop\Programming\WikiEngine\data_files\doc_info.jsonl"

OUTPUT_CSV = "pagelinks.csv"

# Lowered to 4 to prevent disk choking during initialization
NUM_WORKERS = 4 

# --- Regexes ---
LINKTARGET_REGEX = re.compile(r"\(([0-9]+),([0-9]+),'((?:[^'\\]|\\.)*)'\)")
PAGELINKS_REGEX = re.compile(r"\(([0-9]+),([0-9]+),([0-9]+)\)")

# Global Map
target_id_to_page_id = {}

def load_maps_worker():
    global target_id_to_page_id
    pid = os.getpid()
    print(f"[Worker {pid}] Step 1/2: Loading doc_info...", flush=True)
    
    # 1. Load Title -> PageID
    title_to_pid = {}
    count = 0
    try:
        with open(DOC_INFO_FILE, 'r', encoding='utf-8') as f:
            for line in f:
                if not line.strip(): continue
                try:
                    d = json.loads(line)
                    t = d['title'].strip().replace(' ', '_')
                    title_to_pid[t] = d['id']
                    count += 1
                    if count % 2000000 == 0:
                        print(f"[Worker {pid}] Loaded {count} titles...", flush=True)
                except: continue
    except FileNotFoundError:
        print(f"[Worker {pid}] ERROR: doc_info file not found at {DOC_INFO_FILE}")
        return

    print(f"[Worker {pid}] Step 2/2: Parsing linktarget...", flush=True)

    # 2. Load TargetID -> Title -> PageID
    local_tid_map = {}
    valid_targets = 0
    
    if os.path.exists(LINKTARGET_FILE):
        with gzip.open(LINKTARGET_FILE, 'rt', encoding='utf-8', errors='replace') as f:
            for line in f:
                if not line.startswith('INSERT INTO'): continue
                
                for match in LINKTARGET_REGEX.finditer(line):
                    tid, ns, title_raw = match.groups()
                    if ns != '0': continue 
                    
                    title = title_raw.replace(r"\'", "'").replace(r'\"', '"').replace(r'\\', '\\')
                    
                    if title in title_to_pid:
                        local_tid_map[int(tid)] = title_to_pid[title]
                        valid_targets += 1
                        
                    if valid_targets > 0 and valid_targets % 1000000 == 0:
                         print(f"[Worker {pid}] Resolved {valid_targets} targets...", flush=True)
    else:
        print(f"[Worker {pid}] ERROR: linktarget file not found")
        return

    target_id_to_page_id = local_tid_map
    # Clear memory
    del title_to_pid 
    print(f"[Worker {pid}] READY. Mapped {valid_targets} link targets.", flush=True)

def parse_pagelinks_chunk(chunk):
    global target_id_to_page_id
    results = []
    for match in PAGELINKS_REGEX.finditer(chunk):
        from_id, from_ns, target_id = match.groups()
        if from_ns != '0': continue
        
        tid = int(target_id)
        if tid in target_id_to_page_id:
            pid = target_id_to_page_id[tid]
            results.append(f"{from_id},{pid}")
            
    return "\n".join(results)

def generate_chunks(filepath):
    with gzip.open(filepath, 'rt', encoding='utf-8', errors='replace') as f:
        for line in f:
            if not line.startswith('INSERT INTO'): continue
            start = line.find('VALUES ')
            if start != -1: yield line[start:]

if __name__ == "__main__":
    # Quick check for files
    if not os.path.exists(PAGELINKS_FILE): 
        print(f"ERROR: {PAGELINKS_FILE} missing")
        sys.exit()
    if not os.path.exists(LINKTARGET_FILE): 
        print(f"ERROR: {LINKTARGET_FILE} missing")
        sys.exit()
        
    print(f"Starting with {NUM_WORKERS} workers. Please wait for map loading (approx 5-10 mins)...")
    
    start_time = time.time()
    
    with multiprocessing.Pool(NUM_WORKERS, initializer=load_maps_worker) as pool:
        print("Pool created. Reading chunks...")
        with open(OUTPUT_CSV, 'w', encoding='utf-8') as f:
            gen = generate_chunks(PAGELINKS_FILE)
            count = 0
            for chunk_res in pool.imap_unordered(parse_pagelinks_chunk, gen):
                if chunk_res:
                    f.write(chunk_res + '\n')
                    count += chunk_res.count('\n')
                    if count % 100000 == 0: 
                        print(f"Saved {count} links...", end='\r')
    
    print(f"\nDone. Total links saved: {count}")