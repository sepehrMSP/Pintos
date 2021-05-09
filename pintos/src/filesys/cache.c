#include "filesys/cache.h"
#include <list.h>
#include <hash.h>
#include "threads/synch.h"
#include "threads/malloc.h"

struct list cache_list;

// struct hash cache_hash;

// struct lock cache_lock;

struct cache_block
  {
    struct lock lock;

    struct list_elem list_elem;
    struct hash_elem hash_elem;

    block_sector_t sector;
    uint8_t data[BLOCK_SECTOR_SIZE];

    bool dirty;
    int access_count;
  };

// struct cache_block cache;

void
cache_init ()
{
  list_init(&cache_list);
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

void
cache_read (struct block *block, block_sector_t sector, void *buffer)
{
  struct list_elem *e;
  struct cache_block *cache_block;

  for (e = list_begin (&cache_list); e != list_end (&cache_list);
       e = list_next (e))
    {
      cache_block = list_entry (e, struct cache_block, list_elem);
      if (cache_block->sector == sector)
        {
          list_remove(e);
          list_push_front(&cache_list, e);
          break;
        }
    }

  if (e == list_end (&cache_list))
    {
      e = list_prev (e);
      cache_block = list_entry (e, struct cache_block, list_elem);
      block_read(block, sector, cache_block->data);
      cache_block->sector = sector;
      // TODO: check access count somewhere
      
      list_remove(e);
      list_push_front(&cache_list, e);
    }
  memcpy(buffer, cache_block->data, BLOCK_SECTOR_SIZE);
}

void
cache_write (struct block *block, block_sector_t sector, const void *buffer)
{
  block_write(block, sector, buffer);

  struct list_elem *e;
  struct cache_block *cache_block;

  for (e = list_begin (&cache_list); e != list_end (&cache_list);
    e = list_next (e))
  {
    cache_block = list_entry (e, struct cache_block, list_elem);
    if (cache_block->sector == sector)
      {
        cache_block->sector = -1;
        break;
      }
  }
}
