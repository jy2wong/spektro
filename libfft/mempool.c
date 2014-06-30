#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#if defined(_WIN32) || defined(_WIN64)
#include "ntapi.h"
#else
#include <sys/mman.h>
#include <elf.h>
#if defined(__amd64__) || defined(__x86_64__) || defined(__alpha__)
#define ELF_AUXV Elf64_auxv_t
#else
#define ELF_AUXV Elf32_auxv_t
#endif
#endif
unsigned int PageSize = 0;
#define MAX_ALLOC_CHUNK UINT32_MAX

size_t
round_up_pow2 (size_t a, size_t b)
/* a rounded up to the next multiple of b, i.e. ceil(a/b)*b */
/* Assumes a >= 0, b > 0, and b is a power of 2 */
{
  return ((a + b - 1) & (~(b - 1)));
}

#ifndef ALIGN_SIZE		/* so can override from jconfig.h */
#define ALIGN_SIZE  16 /* Most SIMD implementations require this */
#endif

/*
 * We allocate objects from "pools", where each pool is gotten with a single
 * request to jpeg_get_small() or jpeg_get_large().  There is no per-object
 * overhead within a pool, except for alignment padding.  Each pool has a
 * header with a link to the next pool of the same class.
 */

typedef struct small_pool_struct small_pool_ptr;
typedef struct small_pool_struct {
  small_pool_ptr *next;	/* next in list of pools */
  unsigned int bytes_used;		/* how many bytes already used within pool */
  unsigned int bytes_left;		/* bytes still available in this pool */
} small_pool_hdr;

typedef struct large_pool_struct large_pool_ptr;
typedef struct large_pool_struct {
  large_pool_ptr *next; /* next in list of pools */
  size_t bytes_used;        /* how many bytes already used within pool */
} large_pool_hdr;

/*
 * Here is the full definition of a memory manager object.
 */

typedef struct _my_memory_mgr {
  small_pool_ptr *small_list[4];
  size_t total_space_allocated;
} my_memory_mgr;
static my_memory_mgr _mem;

#ifdef MEM_STATS		/* optional extra stuff for statistics */

static void
print_mem_stats (unsigned int pool_id)
{
  my_memory_mgr *mem = &_mem;
  small_pool_ptr *shdr_ptr;
  large_pool_ptr *lhdr_ptr;

  /* Since this is only a debugging stub, we can cheat a little by using
   * fprintf directly rather than going through the trace message code.
   * This is helpful because message parm array can't handle longs.
   */
  fprintf(stderr, "Freeing pool %u, total space = %lu\n",
	      pool_id, mem->total_space_allocated);

  for (lhdr_ptr = mem->large_list; lhdr_ptr != NULL;
       lhdr_ptr = lhdr_ptr->next) {
    fprintf(stderr, "  Large chunk used %lu\n", lhdr_ptr->bytes_used);
  }

  for (shdr_ptr = mem->small_list[pool_id]; shdr_ptr != NULL;
       shdr_ptr = shdr_ptr->next) {
    fprintf(stderr, "  Small chunk used %lu free %lu\n",
            shdr_ptr->bytes_used, shdr_ptr->bytes_left);
  }
}

#endif /* MEM_STATS */

int getpagesize(void) {
    return PageSize;
}

#if defined(_WIN32) || defined(_WIN64)
void setpagesize(void) {
    SYSTEM_BASIC_INFORMATION SystemBasicInfo;
    NtQuerySystemInformation(SystemBasicInformation, &SystemBasicInfo, sizeof(SYSTEM_BASIC_INFORMATION), NULL);
    PageSize = SystemBasicInfo.PageSize;
}
#else
void setpagesize(void *p) {
    /* Pull stuff from the ELF header when possible */
    unsigned long *aux_dat = (unsigned long*)p;

    while (*aux_dat) {
	    ELF_AUXV *auxv_entry = (ELF_AUXV *) aux_dat;
	    if (auxv_entry->a_type == AT_PAGESZ) {
            /* Make certain getpagesize() gives the correct answer */
            PageSize = auxv_entry->a_un.a_val;
	    }
	    aux_dat += 2;
    }
}
#endif

/*
 * Allocation of "small" objects.
 *
 * For these, we use pooled storage.  When a new pool must be created,
 * we try to get enough space for the current request plus a "slop" factor,
 * where the slop will be the amount of leftover space in the new pool.
 * The speed vs. space tradeoff is largely determined by the slop values.
 * A different slop value is provided for each pool class (lifetime),
 * and we also distinguish the first pool of a class from later ones.
 * NOTE: the values given work fairly well on both 16- and 32-bit-int
 * machines, but may be too small if longs are 64 bits or more.
 *
 * Since we do not know what alignment malloc() gives us, we have to
 * allocate ALIGN_SIZE-1 extra space per pool to have room for alignment
 * adjustment.
 */
#define MIN_SLOP 1024 /* greater than 0 to avoid futile looping */

void *
mempool_alloc_small (size_t sizeofobject, unsigned int pool_id)
/* Allocate a "small" object */
{
  my_memory_mgr *mem = &_mem;
  small_pool_ptr *hdr_ptr, *prev_hdr_ptr;
  char * data_ptr;
  size_t min_request, slop = 4096;

  /*
   * Round up the requested size to a multiple of ALIGN_SIZE in order
   * to assure alignment for the next object allocated in the same pool
   * and so that algorithms can straddle outside the proper area up
   * to the next alignment.
   */
  sizeofobject = round_up_pow2(sizeofobject, ALIGN_SIZE);

#if 0
  /* Check for unsatisfiable request (do now to ensure no overflow below) */
  if ((sizeof(small_pool_hdr) + sizeofobject + ALIGN_SIZE - 1) > MAX_ALLOC_CHUNK)
    out_of_memory();	/* request exceeds malloc's ability */
#endif

  /* See if space is available in any existing pool */
  if (pool_id > 3) {
    return NULL;
  }
  prev_hdr_ptr = NULL;
  hdr_ptr = mem->small_list[pool_id];
  while (hdr_ptr != NULL) {
    if (hdr_ptr->bytes_left >= sizeofobject)
      break;			/* found pool with enough space */
    prev_hdr_ptr = hdr_ptr;
    hdr_ptr = hdr_ptr->next;
  }

  /* Time to make a new pool? */
  if (hdr_ptr == NULL) {
    /* min_request is what we need now, slop is what will be leftover */
    min_request = sizeof(small_pool_hdr) + sizeofobject + ALIGN_SIZE - 1;
    if (prev_hdr_ptr == NULL) /* first pool in class? */
        slop = 16384; /* first_pool_slop */
    slop += (4096 - (min_request & 4095));
    /* Don't ask for more than MAX_ALLOC_CHUNK */
    if (slop > (size_t) (MAX_ALLOC_CHUNK-min_request))
      slop = (size_t) (MAX_ALLOC_CHUNK-min_request);
    /* Try to get space, if fail reduce slop and try again */
    for (;;) {
#if defined(_WIN32) || defined(_WIN64)
      SIZE_T sz = (min_request + slop);
      NTSTATUS r = STATUS_SUCCESS;
      hdr_ptr = NULL;
      r = NtAllocateVirtualMemory(((HANDLE)-1), (void**)&hdr_ptr, 0, &sz, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
      if (r == STATUS_SUCCESS)
        break;
#else
      hdr_ptr = (small_pool_ptr*) mmap(NULL, (min_request + slop), PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON|MAP_NORESERVE, -1, 0);
      if (hdr_ptr != NULL)
	    break;
#endif
      slop /= 2;
      if (slop < MIN_SLOP) { /* give up when it gets real small */
        //write(2, "Error: Insufficient Memory\n", 27);
        return NULL;
      }
    }
    mem->total_space_allocated += min_request + slop;
    /* Success, initialize the new pool header and add to end of list */
    hdr_ptr->next = NULL;
    hdr_ptr->bytes_used = 0;
    hdr_ptr->bytes_left = sizeofobject + slop;
    if (prev_hdr_ptr == NULL)	/* first pool in class? */
      mem->small_list[pool_id] = hdr_ptr;
    else
      prev_hdr_ptr->next = hdr_ptr;
  }

  /* OK, allocate the object from the current pool */
  data_ptr = (char *) hdr_ptr; /* point to first data byte in pool... */
  data_ptr += sizeof(small_pool_hdr); /* ...by skipping the header... */
  if ((size_t)data_ptr % ALIGN_SIZE) /* ...and adjust for alignment */
    data_ptr += ALIGN_SIZE - (size_t)data_ptr % ALIGN_SIZE;
  data_ptr += hdr_ptr->bytes_used; /* point to place for object */
  hdr_ptr->bytes_used += sizeofobject;
  hdr_ptr->bytes_left -= sizeofobject;

  return (void *) data_ptr;
}

/*
 * Release all objects belonging to a specified pool.
 */

void
mempool_free_small(unsigned int pool_id)
{
  my_memory_mgr *mem = &_mem;
  small_pool_ptr *shdr_ptr;
  size_t space_freed;

  if (pool_id > 3) {
    return;
  }

#ifdef MEM_STATS
  if (mem->trace_level > 1)
    print_mem_stats(mem); /* print pool's memory usage statistics */
#endif

  /* Release small objects */
  shdr_ptr = mem->small_list[pool_id];
  mem->small_list[pool_id] = NULL;

  while (shdr_ptr != NULL) {
    small_pool_ptr *next_shdr_ptr = shdr_ptr->next;
    space_freed = shdr_ptr->bytes_used + shdr_ptr->bytes_left + sizeof(small_pool_hdr);
#if defined(_WIN32) || defined(_WIN64)
    NtFreeVirtualMemory(((HANDLE)-1), (void**)&shdr_ptr, 0, MEM_RELEASE);
#else
    munmap(shdr_ptr, space_freed);
#endif
    mem->total_space_allocated -= space_freed;
    shdr_ptr = next_shdr_ptr;
  }
}

#if 0
void *
mempool_alloc_large (size_t size)
/* Allocate a "large" object */
{
  void *hdr_ptr = NULL;
  size = round_up_pow2(size, PageSize);

#if defined(_WIN32) || defined(_WIN64)
  NtAllocateVirtualMemory(((HANDLE)-1), (void**)&hdr_ptr, 0, &size, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
#else
  hdr_ptr = (large_pool_ptr*) mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON|MAP_NORESERVE, -1, 0);
#endif
  if (hdr_ptr == NULL) {
    //write(2, "Error: Insufficient Memory\n", 27);
    return NULL;
  }
  return hdr_ptr;
}

void *
mempool_realloc_large (void *ptr, size_t old_size, size_t new_size)
/* Allocate a "large" object */
{
  void *hdr_ptr = NULL;
  old_size = round_up_pow2(old_size, PageSize);
  new_size = round_up_pow2(new_size, PageSize);

#if defined(__linux__)
  hdr_ptr = mremap(ptr, old_size, new_size, 1);
#else
#if defined(_WIN32) || defined(_WIN64)
  NtAllocateVirtualMemory(((HANDLE)-1), (void**)&hdr_ptr, 0, &new_size, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
  memcpy(hdr_ptr, ptr, old_size);
  NtFreeVirtualMemory(((HANDLE)-1), (void**)&ptr, 0, MEM_RELEASE);
#else
  hdr_ptr = (large_pool_ptr*) mmap(NULL, new_size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON|MAP_NORESERVE, -1, 0);
  memcpy(hdr_ptr, ptr, old_size);
  munmap(ptr, old_size);
#endif
#endif
  if (hdr_ptr == NULL) {
    //write(2, "Error: Insufficient Memory\n", 27);
    return NULL;
  }
  return hdr_ptr;
}

void
mempool_free_large(void *ptr, size_t size)
{
  size = round_up_pow2(size, PageSize);
#if defined(_WIN32) || defined(_WIN64)
    NtFreeVirtualMemory(((HANDLE)-1), (void**)&ptr, 0, MEM_RELEASE);
#else
    munmap(ptr, size);
#endif
}
#endif

