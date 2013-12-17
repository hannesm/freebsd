//=== SoftBoundRuntime/softboundcets.c - Creates the main function for SoftBound+CETS Runtime --*- C -*===// 
// Copyright (c) 2011 Santosh Nagarakatte, Milo M. K. Martin. All rights reserved.

// Developed by: Santosh Nagarakatte, Milo M.K. Martin,
//               Jianzhou Zhao, Steve Zdancewic
//               Department of Computer and Information Sciences,
//               University of Pennsylvania
//               http://www.cis.upenn.edu/acg/softbound/

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal with the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

//   1. Redistributions of source code must retain the above copyright notice,
//      this list of conditions and the following disclaimers.

//   2. Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimers in the
//      documentation and/or other materials provided with the distribution.

//   3. Neither the names of Santosh Nagarakatte, Milo M. K. Martin,
//      Jianzhou Zhao, Steve Zdancewic, University of Pennsylvania, nor
//      the names of its contributors may be used to endorse or promote
//      products derived from this Software without specific prior
//      written permission.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
// CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// WITH THE SOFTWARE.
//===---------------------------------------------------------------------===//


#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <sys/mman.h>
#include <softbound.h>

__softboundcets_trie_entry_t** __softboundcets_trie_primary_table;

size_t* __softboundcets_free_map_table = NULL;

size_t* __softboundcets_shadow_stack_ptr = NULL;

size_t* __softboundcets_lock_next_location = NULL;
size_t* __softboundcets_lock_new_location = NULL;
size_t __softboundcets_key_id_counter = 2;

#ifdef __SBCETS_STATS_MODE
size_t __sbcets_stats_memcpy_checks = 0;
size_t __sbcets_stats_metadata_memcopies = 0;
size_t __sbcets_stats_spatial_load_dereference_checks = 0;
size_t __sbcets_stats_spatial_store_dereference_checks = 0;
size_t __sbcets_stats_temporal_load_dereference_checks = 0;
size_t __sbcets_stats_temporal_store_dereference_checks = 0;
size_t __sbcets_stats_metadata_loads = 0;
size_t __sbcets_stats_metadata_stores = 0;
size_t __sbcets_stats_heap_allocations = 0;
size_t __sbcets_stats_stack_allocations = 0;
size_t __sbcets_stats_heap_deallocations = 0;
size_t __sbcets_stats_stack_deallocations = 0;
#endif

/* key 0 means not used, 1 means globals*/
size_t __softboundcets_deref_check_count = 0;
size_t* __softboundcets_global_lock = 0;

size_t* __softboundcets_temporal_space_begin = 0;
size_t* __softboundcets_stack_temporal_space_begin = NULL;


#ifdef __SBCETS_STATS_MODE

static __attribute__ ((__destructor__))
void __sbcets_stats_fini() {

  // 4kB page size, 1024*1024 bytes per MB,
  const double MULTIPLIER = 4096.0/(1024.0*1024.0); 
  FILE* proc_file, *statistics_file;
  size_t total_size_in_pages = 0;
  size_t res_size_in_pages = 0;

  statistics_file = fopen("bench_statistics.log", "w");
  assert(statistics_file != NULL);

  proc_file = fopen("/proc/self/statm", "r");
  fscanf(proc_file, "%zd %zd", &total_size_in_pages, &res_size_in_pages);

  fprintf(statistics_file, "memory_total: %lf \n", total_size_in_pages*MULTIPLIER);
  fprintf(statistics_file, "memory_resident: %lf \n", res_size_in_pages*MULTIPLIER);

  fprintf(statistics_file, "Num_spatial_load_checks:%zd\n",
          __sbcets_stats_spatial_load_dereference_checks);
  fprintf(statistics_file,  "Num_spatial_store_checks:%zd\n", 
          __sbcets_stats_spatial_store_dereference_checks);
  fprintf(statistics_file,  "Num_temporal_load_checks:%zd\n", 
          __sbcets_stats_temporal_load_dereference_checks);
  fprintf(statistics_file,  "Num_temporal_store_checks:%zd\n", 
          __sbcets_stats_temporal_store_dereference_checks);  
  fprintf(statistics_file, "Num_metadata_loads:%zd\n",
          __sbcets_stats_metadata_loads);
  fprintf(statistics_file, "Num_metadata_stores:%zd\n",
          __sbcets_stats_metadata_stores);
  fprintf(statistics_file, "Num_heap_allocations:%zd\n",
          __sbcets_stats_heap_allocations);
  fprintf(statistics_file, "Num_stack_allocations:%zd\n",
          __sbcets_stats_stack_allocations);
  fprintf(statistics_file, "Num_heap_deallocations:%zd\n",
          __sbcets_stats_heap_deallocations);
  fprintf(statistics_file, "Num_stack_deallocations:%zd\n",
          __sbcets_stats_stack_deallocations);
  fprintf(statistics_file, "Num_metadata_memcopies:%zd\n",
          __sbcets_stats_metadata_memcopies);
 fprintf(statistics_file, "Num_memcpy_checks:%zd\n",
          __sbcets_stats_memcpy_checks);
 

  fprintf(statistics_file, 
          "============================================\n");
  fclose(statistics_file);
  
}

#endif

NO_SB_CC __SOFTBOUNDCETS_NORETURN void __softboundcets_abort()
{
  fprintf(stderr, "\nSoftboundcets: Memory safety violation detected\n\nBacktrace:\n");
  fprintf(stderr, "\n\n");
  abort();
}

static int softboundcets_initialized = 0;

NO_SB_CC void __softboundcets_init( int is_trie) 
{
  if (__SOFTBOUNDCETS_DEBUG) {
    __softboundcets_printf("Running __softboundcets_init for module\n");
  }
  
  if (is_trie != __SOFTBOUNDCETS_TRIE) {
    __softboundcets_printf("Softboundcets: Inconsistent specification of metadata encoding\n");
    abort();
  }

  if (softboundcets_initialized != 0) {
    return;  // already initialized, do nothing
  }
  
  softboundcets_initialized = 1;

  if (__SOFTBOUNDCETS_DEBUG) {
    __softboundcets_printf("Initializing softboundcets metadata space\n");
  }

  if(__SOFTBOUNDCETS_TRIE){
    assert(sizeof(__softboundcets_trie_entry_t) >= 16);
  }


  /* Allocating the temporal shadow space */

  size_t temporal_table_length = (__SOFTBOUNDCETS_N_TEMPORAL_ENTRIES)* sizeof(void*);

  __softboundcets_lock_new_location = __softbound_mmap(0, temporal_table_length, 
                                           PROT_READ| PROT_WRITE,
                                           SOFTBOUNDCETS_MMAP_FLAGS, -1, 0);
  
  assert(__softboundcets_lock_new_location != (void*) -1);
  __softboundcets_temporal_space_begin = (size_t *)__softboundcets_lock_new_location;


  size_t stack_temporal_table_length = (__SOFTBOUNDCETS_N_STACK_TEMPORAL_ENTRIES) * sizeof(void*);
  __softboundcets_stack_temporal_space_begin = __softbound_mmap(0, stack_temporal_table_length, 
                                                    PROT_READ| PROT_WRITE, 
                                                    SOFTBOUNDCETS_MMAP_FLAGS, -1, 0);
  assert(__softboundcets_stack_temporal_space_begin != (void*) -1);


  size_t global_lock_size = (__SOFTBOUNDCETS_N_GLOBAL_LOCK_SIZE) * sizeof(void*);
  __softboundcets_global_lock = __softbound_mmap(0, global_lock_size, 
                                     PROT_READ|PROT_WRITE, 
                                     SOFTBOUNDCETS_MMAP_FLAGS, -1, 0);
  assert(__softboundcets_global_lock != (void*) -1);
  //  __softboundcets_global_lock =  __softboundcets_lock_new_location++;
  *((size_t*)__softboundcets_global_lock) = 1;



  size_t shadow_stack_size = __SOFTBOUNDCETS_SHADOW_STACK_ENTRIES * sizeof(size_t);
  __softboundcets_shadow_stack_ptr = __softbound_mmap(0, shadow_stack_size, 
                                          PROT_READ|PROT_WRITE, 
                                          SOFTBOUNDCETS_MMAP_FLAGS, -1, 0);
  assert(__softboundcets_shadow_stack_ptr != (void*)-1);

  *((size_t*)__softboundcets_shadow_stack_ptr) = 0; /* prev stack size */
  size_t * current_size_shadow_stack_ptr =  __softboundcets_shadow_stack_ptr +1 ;
  *(current_size_shadow_stack_ptr) = 0;

  if(__SOFTBOUNDCETS_SHADOW_STACK_DEBUG){
    printf("[mmap_shadow_stack]mmaped shadowstack pointer = %p\n", 
           __softboundcets_shadow_stack_ptr);
  }

  if(__SOFTBOUNDCETS_FREE_MAP) {
    size_t length_free_map = (__SOFTBOUNDCETS_N_FREE_MAP_ENTRIES) * sizeof(size_t);
    __softboundcets_free_map_table = __softbound_mmap(0, length_free_map, 
                                          PROT_READ| PROT_WRITE, 
                                          SOFTBOUNDCETS_MMAP_FLAGS, -1, 0);
    assert(__softboundcets_free_map_table != (void*) -1);
  }

  if(__SOFTBOUNDCETS_TRIE) {
    size_t length_trie = (__SOFTBOUNDCETS_TRIE_PRIMARY_TABLE_ENTRIES) * sizeof(__softboundcets_trie_entry_t*);

    __softboundcets_trie_primary_table = __softbound_mmap(0, length_trie,
                                              PROT_READ| PROT_WRITE,
                                              SOFTBOUNDCETS_MMAP_FLAGS, -1, 0);
    assert(__softboundcets_trie_primary_table != (void *)-1);

    int* temp = __malloc(1);
    __softboundcets_allocation_secondary_trie_allocate_range(0, (size_t)temp);

    return;
  }


}

NO_SB_CC void __softboundcets_printf(const char* str, ...) {
  /*  va_list args;

  va_start(args, str);
  vfprintf(stderr, str, args);
  va_end(args);
  */
}

NO_SB_CC __WEAK_INLINE void
__softboundcets_read_shadow_stack_metadata_store(char** endptr, int arg_num){
    void* nptr_base = __softboundcets_load_base_shadow_stack(arg_num);
    void* nptr_bound = __softboundcets_load_bound_shadow_stack(arg_num);
    size_t nptr_key = __softboundcets_load_key_shadow_stack(arg_num);
    void* nptr_lock = __softboundcets_load_lock_shadow_stack(arg_num);
    __softboundcets_metadata_store(endptr, nptr_base, nptr_bound, nptr_key,
                                   nptr_lock);
}

NO_SB_CC __WEAK_INLINE void
__softboundcets_propagate_metadata_shadow_stack_from(int from_argnum,
                                                     int to_argnum){
  void* base = __softboundcets_load_base_shadow_stack(from_argnum);
  void* bound = __softboundcets_load_bound_shadow_stack(from_argnum);
  size_t key = __softboundcets_load_key_shadow_stack(from_argnum);
  void* lock = __softboundcets_load_lock_shadow_stack(from_argnum);
  __softboundcets_store_base_shadow_stack(base, to_argnum);
  __softboundcets_store_bound_shadow_stack(bound, to_argnum);
  __softboundcets_store_key_shadow_stack(key, to_argnum);
  __softboundcets_store_lock_shadow_stack(lock, to_argnum);
}

NO_SB_CC __WEAK_INLINE void __softboundcets_store_null_return_metadata(){
  __softboundcets_store_base_shadow_stack(NULL, 0);
  __softboundcets_store_bound_shadow_stack(NULL, 0);
  __softboundcets_store_key_shadow_stack(0, 0);
  __softboundcets_store_lock_shadow_stack(NULL, 0);
}

NO_SB_CC __WEAK_INLINE void
__softboundcets_store_return_metadata(void* base, void* bound, size_t key,
                                      void* lock){

  __softboundcets_store_base_shadow_stack(base, 0);
  __softboundcets_store_bound_shadow_stack(bound, 0);
  __softboundcets_store_key_shadow_stack(key, 0);
  __softboundcets_store_lock_shadow_stack(lock, 0);
}
