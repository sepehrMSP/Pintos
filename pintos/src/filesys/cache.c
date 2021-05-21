#include "filesys/cache.h"
#include <list.h>
#include <hash.h>
#include <string.h>
#include "threads/synch.h"
#include "threads/malloc.h"

#ifdef ENABLE_CACHE

struct block *fs_device = NULL;

struct list cache_list;

struct hash cache_hash;

// struct lock cache_lock;

struct cache_block
  {
    struct lock lock; // not used yet

    struct list_elem list_elem;
    struct hash_elem hash_elem;

    block_sector_t sector;
    uint8_t data[BLOCK_SECTOR_SIZE];

    bool dirty;
    int access_count; // not used yet
  };

unsigned
hash_func (const struct hash_elem *e, void *aux)
{
  const struct cache_block *block = hash_entry(e, struct cache_block, hash_elem);
  return block->sector;
}

bool
hash_neq_func (const struct hash_elem *a, const struct hash_elem *b, void *aux)
{
  const struct cache_block *a_block = hash_entry(a, struct cache_block, hash_elem);
  const struct cache_block *b_block = hash_entry(b, struct cache_block, hash_elem);
  return a_block->sector != b_block->sector;
}

void
cache_init (void)
{
  list_init(&cache_list);
  hash_init(&cache_hash, hash_func, hash_neq_func, NULL);
  for (int i = 0; i < CACHE_SIZE; i++)
    {
      struct cache_block *block = malloc(sizeof(struct cache_block));
      lock_init(&block->lock);
      block->dirty = false;
      block->access_count = 0;
      block->sector = -1; // TODO: remove me
      list_push_front(&cache_list, &block->list_elem);
    }
}

static void
cache_out (struct cache_block *cache_block)
{
  ASSERT (fs_device != NULL);
  if (hash_find (&cache_hash, &cache_block->hash_elem))
    {
      if (cache_block->dirty)
        block_write(fs_device, cache_block->sector, cache_block->data);
      hash_delete(&cache_hash, &cache_block->hash_elem);
    }
}

void
cache_read (struct block *block, block_sector_t sector, void *buffer)
{
  if (!fs_device)
    fs_device = block;
  ASSERT (fs_device == block);

  struct cache_block *cache_block;
  struct cache_block search_block;
  struct hash_elem *h;

  search_block.sector = sector;
  h = hash_find (&cache_hash, &search_block.hash_elem);

  if (h != NULL)
    {
      cache_block = hash_entry (h, struct cache_block, hash_elem);
    }
  else
    {
      struct list_elem *e;
      e = list_rbegin(&cache_list);
      cache_block = list_entry (e, struct cache_block, list_elem);
      cache_out(cache_block);
      block_read(block, sector, cache_block->data);
      cache_block->sector = sector;
      hash_insert(&cache_hash, &cache_block->hash_elem);
      // TODO: check access count somewhere  
    }

  list_remove(&cache_block->list_elem);
  list_push_front(&cache_list, &cache_block->list_elem);
  memcpy(buffer, cache_block->data, BLOCK_SECTOR_SIZE);
}

void
cache_write (struct block *block, block_sector_t sector, const void *buffer)
{
  if (!fs_device)
    fs_device = block;
  ASSERT (fs_device == block);

  struct cache_block *cache_block;
  struct cache_block search_block;
  struct hash_elem *h;

  search_block.sector = sector;
  h = hash_find (&cache_hash, &search_block.hash_elem);

  if (h != NULL)
    {
      cache_block = hash_entry (h, struct cache_block, hash_elem);
    }
  else
    {
      struct list_elem *e;
      e = list_rbegin(&cache_list);
      cache_block = list_entry (e, struct cache_block, list_elem);
      cache_out(cache_block);
      cache_block->sector = sector;
      hash_insert(&cache_hash, &cache_block->hash_elem);
      // TODO: check access count somewhere  
    }

  list_remove(&cache_block->list_elem);
  list_push_front(&cache_list, &cache_block->list_elem);
  memcpy(cache_block->data, buffer, BLOCK_SECTOR_SIZE);
  cache_block->dirty = true;
}

void
cache_flush ()
{
  while (list_size (&cache_list))
    {
      struct list_elem *e = list_pop_front (&cache_list);
      struct cache_block *cache_block = list_entry (e, struct cache_block, list_elem);
      if (cache_block->dirty)
        cache_out (cache_block);
    }
}

#else

void
cache_init (void)
{
  return;
}

void
cache_read (struct block *block, block_sector_t sector, void *buffer)
{
  block_read(block, sector, buffer);
}

void
cache_write (struct block *block, block_sector_t sector, const void *buffer)
{
  block_write(block, sector, buffer);
}

void
cache_flush ()
{
  return;
}

#endif
