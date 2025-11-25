import mwxml
import mwparserfromhell
import sys
from nltk.tokenize import word_tokenize
from nltk.corpus import stopwords
from nltk.stem import PorterStemmer
import json
from collections import Counter
import time
import multiprocessing
import os
from typing import Tuple, Optional, Generator
import bz2


try:
    english_stopwords = set(stopwords.words('english'))
    porter = PorterStemmer()

except LookupError:
    print("--- Error nltk data: Stopwords or punkt, not found ---")
    sys.exit()

def process_article(page_data: Tuple[int, str, str]) -> Optional[Tuple[str, str]]:
    doc_id, title, raw_text = page_data
    try:
        wikicode = mwparserfromhell.parse(raw_text)

        for template in list(wikicode.filter_templates()):
            try:
                 wikicode.remove(template)
            except ValueError:
                pass

        for link in list(wikicode.filter_wikilinks()):
            is_image = False
            title_str = str(link.title).strip()
            if title_str.startswith("File:"):
                is_image = True
            elif title_str.startswith("Image:"):
                is_image = True
            
            if is_image:
                try:
                    wikicode.remove(link)
                except ValueError:
                    pass
            else:
                continue

        clean_text = wikicode.strip_code()
        clean_text = clean_text.lower()

        tokens = word_tokenize(clean_text)
        master_tokens = tokens + word_tokenize(clean_text.replace('-', ' '))
        final_stemmed_list = [porter.stem(token) for token in master_tokens if len(token) > 1 and token.isalnum() and not token.isdigit() and token not in english_stopwords]
        doc_length = len(final_stemmed_list)

        term_freqs = Counter(final_stemmed_list)

        tf_scores = {}
        if doc_length > 0:
            for term, count in term_freqs.items():
                tf_scores[term] = count / doc_length
        
        tf_data_data = {"id":doc_id, "terms":tf_scores}
        tf_data_json_string = json.dumps(tf_data_data)
        
        doc_info_data = {"id":doc_id, "title": title, "len": doc_length}
        doc_info_json_string = json.dumps(doc_info_data)

        return (tf_data_json_string, doc_info_json_string)
    
    except Exception as e:
        print(f"\n[!] Error occured while proccesing {doc_id} ({title}): {e}")

def page_generator(dump_path: str, processed_ids_set: set) -> Generator[Tuple[int, str, str], None, None]:

    dump_file = None

    try:
        dump_file = bz2.open(dump_path, mode='rt', encoding='utf-8', errors='replace')
        dump = mwxml.Dump.from_file(dump_file)

        parsed = 0

        for page in dump:

            if page.id in processed_ids_set:
                parsed += 1
                if(parsed % 1000 == 0):
                    print(f"{parsed} pages already parsed.")
                continue

            if page.redirect:
                continue

            if page.namespace != 0:
                continue

            try:
                revision = next(page)
            except StopIteration:
                continue

            if not (revision and revision.text):
                continue

            yield(page.id, page.title, revision.text)

    except FileNotFoundError:
        print(f"\n--- [Producer] Error: Dump file not found ---")
    except Exception as e:
        print(f"\n--- [Producer] Error: {e} ---") 
    finally:

        if dump_file:
            dump_file.close()

            print(f"\n---------- [Producer] Work generator has finished. ----------")

        

if __name__ == "__main__":
    NUM_WORKERS = 10
    DUMP_FILE_PATH = r"C:\Users\Arnav\Desktop\Programming\WikiEngine\enwiki-20251101-pages-articles-multistream.xml.bz2"
    TF_FILE = "tf_data.jsonl"
    DOC_FILE = "doc_info.jsonl"
    STOP_FILE = "stop.txt"

    processed_ids_set = set()

    if os.path.exists(DOC_FILE):
        with open(DOC_FILE, 'r', encoding='utf-8') as file:
            for line in file:
                if not line.strip():
                    continue
                
                try:
                    data = json.loads(line)

                    processed_ids_set.add(data['id'])
                except json.JSONDecodeError:
                    print(f"[Main] Warning: Found a corrupt line in {DOC_FILE}. Skipping it.")
            
        print(f"[Main] Resume set built. Found {len(processed_ids_set)} processed articles.")
    
    else:
        print(f"[Main] No {DOC_FILE} found. Starting a new run.")        
        
    start_time = time.time()
    prev_time = start_time
    total_processed_count_in_this_run = 0

    try:
        pool = multiprocessing.Pool(processes=NUM_WORKERS)

        work_generator = page_generator(DUMP_FILE_PATH, processed_ids_set)

        with open(TF_FILE, 'a', encoding='utf-8') as tf_file:
            with open(DOC_FILE, 'a', encoding='utf-8') as doc_file:
                print("[Main] Pool started. Waiting for results...")

                for result_tuple in pool.imap_unordered(process_article, work_generator):

                    if result_tuple is None:
                        continue
                    
                    tf_data_json_string, doc_info_json_string = result_tuple
                    tf_file.write(tf_data_json_string + '\n')
                    doc_file.write(doc_info_json_string + '\n')

                    total_processed_count_in_this_run += 1

                    if total_processed_count_in_this_run % 1000 == 0:
                        curr_time = time.time()
                        
                        total_seconds = curr_time - start_time
                        total_minutes, divided_total_seconds = divmod(total_seconds, 60)
                        average_speed = total_processed_count_in_this_run / total_seconds

                        section_seconds = curr_time - prev_time
                        section_minutes, divided_section_seconds = divmod(section_seconds, 60)
                        section_speed = 1000 / section_seconds

                        print(f"[Main] Processed {total_processed_count_in_this_run} new articles. Time: {int(total_minutes)} min, {divided_total_seconds:.2f} sec. Avg. Speed: {average_speed:.2f} pages/second. Section Speed: {section_speed:.2f} pages/second")
                        prev_time = curr_time

    except KeyboardInterrupt:
        print("\n[Main] Ctrl+C detected! Shutting down pool...")
        if pool is not None:
            try:
                pool.terminate()  # kill workers now
            except Exception as e:
                print(f"[Main] Error terminating pool: {e}")
            finally:
                pool.join()
        print("[Main] Workers terminated by user (Ctrl+C).")
    except Exception as e:
        print(f"\n[Main] A critical error occurred: {e}")
        if pool is not None:
            try:
                print("[Main] Terminating worker pool due to critical error...")
                pool.terminate()
            except Exception as e2:
                print(f"[Main] Error terminating pool after critical error: {e2}")
            finally:
                pool.join()
    finally:
        print("\n[Main] Telling worker pool to stop...")

        if pool is not None:
            # If pool is still alive, try graceful close -> join
            try:
                # pool._state == multiprocessing.pool.TERMINATE or CLOSE is internals, so we just attempt close()
                pool.close()
                pool.join()
            except Exception:
                # If close/join fails (e.g. because we already terminated), ignore
                pass

        print("[Main] Shutdown complete.")

        end_time = time.time()
        elapsed_seconds = end_time - start_time
        elapsed_minutes, seconds = divmod(elapsed_seconds, 60)

        print(f"\n---------- Processing Complete! ----------")
        print(f"    Total articles processed in this run: {total_processed_count_in_this_run}")
        print(f"    Total articles in index: {len(processed_ids_set) + total_processed_count_in_this_run}")
        print(f"    Total time elapsed: {int(elapsed_minutes)} min, {seconds:.2f} sec")


