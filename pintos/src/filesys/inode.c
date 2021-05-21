#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "filesys/cache.h"
#include "threads/malloc.h"
#include "threads/synch.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

#ifndef UNIXFFS
  #define UNIXFFS
#endif
/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
#ifndef UNIXFFS
struct inode_disk
  {
    block_sector_t start;               /* First data sector. */
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
    uint32_t unused[125];               /* Not used. */
  };
#else
  #define DIRECT_REGION_BOUND 122
  #define INDIRECT1_REGION_BOUND (DIRECT_REGION_BOUND + 128)
  #define INDIRECT2_REGION_BOUND (INDIRECT1_REGION_BOUND + 128*128)

  struct inode_disk
    {
      off_t length;
      unsigned magic;
      bool is_dir;
      bool unused[3];
      block_sector_t parent_dir;
      block_sector_t direct[DIRECT_REGION_BOUND];
      block_sector_t indirect;
      block_sector_t doubly_indirect;
    };
#endif

struct lock lock_inode_list;


/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    #ifndef UNIXFFS
      struct inode_disk data;             /* Inode content. */
    #else
      struct inode_disk *data;
    #endif
    bool is_dir;
    struct lock lock;
  };

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos)
{
  ASSERT (inode != NULL);
  #ifndef UNIXFFS
    if (pos < inode->data.length)
      return inode->data.start + pos / BLOCK_SECTOR_SIZE;
    else
      return -1;
  #else
    if (pos < inode->data->length)
      {
        size_t sector_num = pos / BLOCK_SECTOR_SIZE;
        if (sector_num < DIRECT_REGION_BOUND) 
          {
            return inode->data->direct[sector_num];
          }
        else if (sector_num < INDIRECT1_REGION_BOUND) 
          {
            sector_num -= DIRECT_REGION_BOUND;
            block_sector_t layer1 = inode->data->indirect;
            block_sector_t buffer[BLOCK_SECTOR_SIZE_int];
            cache_read (fs_device, layer1, (void *)buffer);
            return buffer[sector_num];
          }
        else if (sector_num < INDIRECT2_REGION_BOUND) 
          {
            sector_num = sector_num - INDIRECT1_REGION_BOUND;
            block_sector_t layer1 = inode->data->doubly_indirect;
            block_sector_t buffer[BLOCK_SECTOR_SIZE_int];
            cache_read(fs_device, layer1, (void *)buffer);
            block_sector_t layer2 = buffer[sector_num / 128];
            cache_read(fs_device, layer2, buffer);
            return buffer[sector_num % 128];
          }
      }
    else
      {
        return -1;
      }   
  #endif
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void)
{
  list_init (&open_inodes);
  cache_init ();
  lock_init (&lock_inode_list);
}


/* Reads the inode_disk located at sector SECTOR and frees all allocated blocks up until FAILED_SECTOR */
void
roll_back(block_sector_t sector, int start_sector, int failed_sector, bool indirect_alloc, bool dbl_indirect_alloc, bool layer1_alloc[])
{
  struct inode_disk disk_inode;
  cache_read(fs_device, sector, &disk_inode);
  if (start_sector == 0)
    free_map_release(sector, 1);
  int i = start_sector;
  for (; i < failed_sector && i < DIRECT_REGION_BOUND; i++)
    {
      free_map_release(disk_inode.direct[i], 1);
    }
  if (failed_sector >= DIRECT_REGION_BOUND && indirect_alloc)
    {
      int indirect[BLOCK_SECTOR_SIZE_int];
      cache_read(fs_device, disk_inode.indirect, (void *) indirect);
      free_map_release(disk_inode.indirect, 1);
      for (; i < failed_sector && i < INDIRECT2_REGION_BOUND; i++)
        {
          free_map_release(indirect[i - INDIRECT1_REGION_BOUND], 1);
        }
    }
  
  if (failed_sector >= INDIRECT1_REGION_BOUND && dbl_indirect_alloc)
    {
      int layer1[BLOCK_SECTOR_SIZE_int];
      int layer2[BLOCK_SECTOR_SIZE_int];
      cache_read(fs_device, disk_inode.doubly_indirect, (void *) layer1);
      free_map_release(disk_inode.doubly_indirect, 1);
      int layer_num = 0;
      for (; i < failed_sector; i++)
        {
          if (!layer1_alloc[layer_num])
            {
              break;
            }
          cache_read(fs_device, layer1[layer_num], (void *)layer2);
          free_map_release(layer1[layer_num], 1);
          for (int layer_index = 0; i < failed_sector && layer_index < BLOCK_SECTOR_SIZE_int; i++, layer_index++)
            {
              free_map_release(layer2[layer_index], 1);
            }
          layer_num++;
        }
    }
}

#ifdef UNIXFFS
bool inode_extend(struct inode *inode, off_t length)
{
  size_t new_sectors = bytes_to_sectors(length);
  if (new_sectors > INDIRECT2_REGION_BOUND)
    {
      return false;
    }
  size_t cur_sectors = bytes_to_sectors(inode_length(inode));
  if (new_sectors == cur_sectors)
    {
      inode->data->length = length;
      cache_write(fs_device, inode->sector, inode->data);
      return true;
    }
  static char zeros[BLOCK_SECTOR_SIZE];
  static block_sector_t magic[BLOCK_SECTOR_SIZE_int];
  for (int i = 0; i < BLOCK_SECTOR_SIZE_int; i++)
    magic[i] = INODE_MAGIC;
  struct inode_disk *disk_inode = inode->data;
  bool rollback = false;
  bool indirect_alloc = false;
  bool dbl_indr_alloc = false;
  bool layer1_alloc[BLOCK_SECTOR_SIZE_int];
  memset(layer1_alloc, 0, BLOCK_SECTOR_SIZE_int * sizeof(bool));
  size_t i;

  for (i = cur_sectors; i < DIRECT_REGION_BOUND && i < new_sectors; i++)
    {
      if (!free_map_allocate(1, disk_inode->direct + i))
        {
          rollback = true;
          cache_write(fs_device, inode->sector, disk_inode);
          goto fail_extend;
        }
      cache_write(fs_device, disk_inode->direct[i], zeros);
    }

  if (i >= DIRECT_REGION_BOUND && i < new_sectors)
    {
      if (disk_inode->indirect == INODE_MAGIC)
        {
          if (!free_map_allocate(1, &disk_inode->indirect))
            {
              rollback = true;
              cache_write(fs_device, inode->sector, disk_inode);
              goto fail_extend;
            }
          indirect_alloc = true;
          cache_write(fs_device, inode->sector, disk_inode);
        }
      block_sector_t buffer[BLOCK_SECTOR_SIZE_int];
      cache_read(fs_device, disk_inode->indirect, (void *) buffer);
      for (; i < INDIRECT1_REGION_BOUND && i < new_sectors; i++)
        {
          if (!free_map_allocate(1, buffer + i - DIRECT_REGION_BOUND))
            {
              rollback = true;
              cache_write(fs_device, disk_inode->indirect, (void *) buffer);
              cache_write(fs_device, inode->sector, disk_inode);
              goto fail_extend;
            }
          cache_write(fs_device, buffer[i - DIRECT_REGION_BOUND], zeros);
        }
      cache_write(fs_device, disk_inode->indirect, (void *) buffer);
    }

  if (i >= INDIRECT1_REGION_BOUND && i < new_sectors)
    {
      if (disk_inode->doubly_indirect == INODE_MAGIC)
        {
          if (!free_map_allocate(1, &disk_inode->doubly_indirect))
            {
              rollback = true;
              cache_write(fs_device, inode->sector, disk_inode);
              goto fail_extend;
            }
          cache_write(fs_device, inode->sector, disk_inode);
          cache_write(fs_device, disk_inode->doubly_indirect, (void *) magic);
          dbl_indr_alloc = true;
        }

      size_t layer_num = (i - INDIRECT1_REGION_BOUND) / 128;
      block_sector_t buffer_l1[BLOCK_SECTOR_SIZE_int];
      cache_read(fs_device, disk_inode->doubly_indirect, (void *) buffer_l1);
      block_sector_t buffer_l2[BLOCK_SECTOR_SIZE_int];
      while (i < new_sectors)
        {
          if (buffer_l1[layer_num] == INODE_MAGIC)
            {
              if (!free_map_allocate(1, &buffer_l1[layer_num]))
                {
                  rollback = true;
                  cache_write(fs_device, disk_inode->doubly_indirect, (void *) buffer_l1);
                  cache_write(fs_device, inode->sector, disk_inode);
                  goto fail_extend;
                }
              cache_write(fs_device, buffer_l1[layer_num], (void *) magic);
              layer1_alloc[layer_num] = true;
            }

          cache_read(fs_device, buffer_l1[layer_num], (void *) buffer_l2);
          for (int layer_index = (i - INDIRECT1_REGION_BOUND) % 128; layer_index < BLOCK_SECTOR_SIZE_int && i < new_sectors; layer_index++, i++)
            {
              if (buffer_l2[layer_index] == INODE_MAGIC)
                {
                  if (!free_map_allocate(1, buffer_l2 + layer_index))
                    {
                      rollback = true;
                      cache_write(fs_device, buffer_l1[layer_num], (void *) buffer_l2);
                      cache_write(fs_device, disk_inode->doubly_indirect, (void *) buffer_l1);
                      cache_write(fs_device, inode->sector, disk_inode);
                      goto fail_extend;
                    }
                  cache_write(fs_device, buffer_l2[layer_index], zeros);
                }
            }
          cache_write(fs_device, buffer_l1[layer_num], (void *) buffer_l2);
          layer_num++;
        }
      cache_write(fs_device, disk_inode->doubly_indirect, (void *) buffer_l1);
    }

  inode->data->length = length;
  cache_write(fs_device, inode->sector, disk_inode);

fail_extend:
  if (rollback)
  {
    roll_back(inode->sector, cur_sectors, i, indirect_alloc, dbl_indr_alloc, layer1_alloc);
    return false;
  }

  return true;
}
#endif

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length, bool is_dir)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
      size_t sectors = bytes_to_sectors (length);
      disk_inode->magic = INODE_MAGIC;
      #ifndef UNIXFFS
      disk_inode->length = length;
        if (free_map_allocate (sectors, &disk_inode->start))
          {
            cache_write (fs_device, sector, disk_inode);
            if (sectors > 0)
              {
                static char zeros[BLOCK_SECTOR_SIZE];
                size_t i;

                for (i = 0; i < sectors; i++)
                  cache_write (fs_device, disk_inode->start + i, zeros);
              }
            success = true;
          }
      #else
        if (sectors >= INDIRECT2_REGION_BOUND)
          return -1;

        disk_inode->length = 0;
        disk_inode->indirect = INODE_MAGIC;
        disk_inode->doubly_indirect = INODE_MAGIC;
        disk_inode->is_dir = is_dir;

        struct inode inode;
        inode.sector = sector;
        inode.data = disk_inode;
        lock_init (&inode.lock);

        success = inode_extend(&inode, length);

      #endif
      free (disk_inode);
    }
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  lock_acquire (&lock_inode_list);
  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e))
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector)
        {
          inode_reopen (inode);
          lock_release (&lock_inode_list);
          return inode;
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    {
      lock_release (&lock_inode_list);
      return NULL;
    }

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  lock_init (&inode->lock);
  #ifndef UNIXFFS
    cache_read (fs_device, inode->sector, &inode->data);
  #else
    inode->data = malloc(BLOCK_SECTOR_SIZE);
    cache_read (fs_device, inode->sector, (void *) inode->data);
  #endif
  lock_release (&lock_inode_list);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  lock_acquire (&inode->lock);
  if (inode != NULL)
    inode->open_cnt++;
  lock_release (&inode->lock);
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  lock_acquire (&inode->lock);
  block_sector_t ret = inode->sector;
  lock_release (&inode->lock);
  return ret;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode)
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;
  lock_acquire (&inode->lock);
  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      lock_acquire (&lock_inode_list);
      list_remove (&inode->elem);
      lock_release (&lock_inode_list);
      /* Deallocate blocks if removed. */
      if (inode->removed)
        {
          #ifndef UNIXFFS
            free_map_release (inode->sector, 1);
            free_map_release (inode->data.start,
                            bytes_to_sectors (inode->data.length));
          #else
            bool layer1_alloc[BLOCK_SECTOR_SIZE_int];
            memset (layer1_alloc, 1, (sizeof (bool)) * BLOCK_SECTOR_SIZE_int);
            roll_back (inode->sector, 0, bytes_to_sectors(inode->data->length), true, true, layer1_alloc);
          #endif
        }
      #ifdef UNIXFFS
        free(inode->data);
      #endif
      lock_release (&inode->lock);
      free (inode);
      return;
    }
  lock_release (&inode->lock);
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode)
{
  lock_acquire (&inode->lock);
  ASSERT (inode != NULL);
  inode->removed = true;
  lock_release (&inode->lock);
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset)
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;
  lock_acquire (&inode->lock);
  while (size > 0)
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Read full sector directly into caller's buffer. */
          cache_read (fs_device, sector_idx, buffer + bytes_read);
        }
      else
        {
          /* Read sector into bounce buffer, then partially copy
             into caller's buffer. */
          if (bounce == NULL)
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }
          cache_read (fs_device, sector_idx, bounce);
          memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  free (bounce);
  lock_release (&inode->lock);
  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset)
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;
  lock_acquire (&inode->lock);
  if (inode->deny_write_cnt)
    {
      lock_release (&inode->lock);
      return 0;
    }

  #ifdef UNIXFFS
    off_t new_length = offset + size;
    if (new_length > inode_length (inode) && !inode_extend (inode, new_length))
      {
        lock_release (&inode->lock);
        return 0;
      }
  #endif

  while (size > 0)
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Write full sector directly to disk. */
          cache_write (fs_device, sector_idx, buffer + bytes_written);
        }
      else
        {
          /* We need a bounce buffer. */
          if (bounce == NULL)
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }

          /* If the sector contains data before or after the chunk
             we're writing, then we need to read in the sector
             first.  Otherwise we start with a sector of all zeros. */
          if (sector_ofs > 0 || chunk_size < sector_left)
            cache_read (fs_device, sector_idx, bounce);
          else
            memset (bounce, 0, BLOCK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          cache_write (fs_device, sector_idx, bounce);
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  free (bounce);
  lock_release (&inode->lock);
  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode)
{
  lock_acquire (&inode->lock);
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  lock_release (&inode->lock);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode)
{
  lock_acquire (&inode->lock);
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
  lock_release (&inode->lock);
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  #ifndef UNIXFFS
    return inode->data.length;
  #else
    bool has_lock = lock_held_by_current_thread (&inode->lock);
    if (!has_lock)
      lock_acquire (&inode->lock);
    off_t ret = inode->data->length;
    if (!has_lock)
      lock_release (&inode->lock);
    return ret;
  #endif
}

// Our Change
/* Returns the sector of the inode */
block_sector_t
get_inode_sector (struct inode *inode)
{
  lock_acquire (&inode->lock);
  block_sector_t ret = inode->sector;
  lock_release (&inode->lock);
  return ret;
}

bool
inode_is_dir(struct inode *inode)
{
  lock_acquire (&inode->lock);
  bool ret = inode->data->is_dir;
  lock_release (&inode->lock);
  return ret;
}

block_sector_t
get_inode_parent_sector(struct inode *inode)
{
  lock_acquire (&inode->lock);
  block_sector_t ret = inode->data->parent_dir;
  lock_release (&inode->lock);
  return ret;
}

void
set_inode_parent (block_sector_t parent_sector, block_sector_t inode_sector)
{
  struct inode *inode = inode_open (inode_sector);
  lock_acquire (&inode->lock);
  inode->data->parent_dir = parent_sector;
  cache_write(fs_device, inode_sector, inode->data);
  lock_release (&inode->lock);
  inode_close (inode);
}

int
get_inode_open_cnt (struct inode *inode)
{
  lock_acquire (&inode->lock);
  int ret = inode->open_cnt;
  lock_release (&inode->lock);
  return ret;
}

void
decrement_inode_open_cnt (struct inode *inode)
{
  lock_acquire (&inode->lock);
  inode->open_cnt --;
  lock_release (&inode->lock);
}

void
increment_inode_open_cnt (struct inode *inode)
{
  lock_acquire (&inode->lock);
  inode->open_cnt ++;
  lock_release (&inode->lock);
}