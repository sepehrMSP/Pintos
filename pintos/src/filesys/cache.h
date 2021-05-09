#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include <stdbool.h>
#include "devices/block.h"

#define CACHE_SIZE 64

void cache_init ();
void cache_read (struct block *, block_sector_t, void *);
void cache_write (struct block *, block_sector_t, const void *);

#endif /* filesys/cache.h */