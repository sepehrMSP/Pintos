#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "filesys/cache.h"
#include "threads/malloc.h"

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
  #define DIRECT_REGION_BOUND 124
  #define INDIRECT1_REGION_BOUND 252
  #define INDIRECT2_REGION_BOUND 16636

  struct inode_disk
    {
      off_t length;
      unsigned magic;
      block_sector_t direct[124];
      block_sector_t indirect;
      block_sector_t doubly_indirect;
    };
#endif
/**
inode changes:

->data
->sector
**/



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
}


void
roll_back(block_sector_t sector, int failed_sector, bool indirect_alloc, bool dbl_indirect_alloc, bool layer1_alloc[])
{
  struct inode_disk disk_inode;
  cache_read(fs_device, sector, &disk_inode);
  free_map_release(sector, 1);
  int i = 0;
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
          for (int index = 0; i < failed_sector && index < BLOCK_SECTOR_SIZE_int; i++, index++)
            {
              free_map_release(layer2[index], 1);
            }
          layer_num++;
        }
    }
}


/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length)
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
      disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;
      #ifndef UNIXFFS
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
        int i = 0;
        bool rollback = false, indirect_alloc = false, dbl_indr_alloc = false;
        bool layer1_alloc[BLOCK_SECTOR_SIZE_int];
        memset(layer1_alloc, 0, BLOCK_SECTOR_SIZE_int * sizeof(block_sector_t));
        static char zeros[BLOCK_SECTOR_SIZE];

        for (; i < DIRECT_REGION_BOUND && i < sectors; i++) 
          {
            if (!free_map_allocate(1, disk_inode->direct + i))
              {
                rollback = true;
                goto fail;
              }
            cache_write(fs_device, disk_inode->direct[i], zeros);
          }

        if (i >= DIRECT_REGION_BOUND)
          {
            if (!free_map_allocate(1, &disk_inode->indirect))
              {
                rollback = true;
                goto fail;
              }
            indirect_alloc = true;
            int buffer[BLOCK_SECTOR_SIZE_int];
            for (; i < INDIRECT1_REGION_BOUND && i < sectors; i++) 
              {
                if (!free_map_allocate(1, buffer + i))
                  {
                    rollback = true;
                    goto fail;
                  }
                cache_write(fs_device, disk_inode->direct[i], zeros);
              }
            cache_write(fs_device, disk_inode->indirect, (void *) buffer);
          }

        // ATTENTION! MINE FIELD!

        if (i >= INDIRECT1_REGION_BOUND)
          {
            if (!free_map_allocate(1, &disk_inode->doubly_indirect))
              {
                rollback = true;
                goto fail;
              }
            dbl_indr_alloc = true;

            int layer_num = 0;
            int buffer_l1[BLOCK_SECTOR_SIZE_int];
            int buffer_l2[BLOCK_SECTOR_SIZE_int];
            int index;
            while (i < sectors)
              {
                if (!free_map_allocate(1, buffer_l1 + layer_num))
                  {
                    rollback = true;
                    goto fail;
                  }
                layer1_alloc[layer_num] = true;
                for (; i < INDIRECT1_REGION_BOUND + (layer_num + 1) * BLOCK_SECTOR_SIZE_int && i < sectors; i++) 
                  {
                    index = i - INDIRECT1_REGION_BOUND - layer_num * BLOCK_SECTOR_SIZE_int;
                    if (!free_map_allocate(1, buffer_l2 + index))
                      {
                       rollback = true;
                       goto fail;
                     }
                    cache_write(fs_device, buffer_l2[index], zeros);
                  }
                cache_write(fs_device, buffer_l1[layer_num], (void *) buffer_l2);
                layer_num++;
              }
            cache_write(fs_device, disk_inode->doubly_indirect, (void *) buffer_l1);
          }

          cache_write(fs_device, sector, disk_inode);
fail:
        if (rollback)
          {
            roll_back(sector, i, indirect_alloc, dbl_indr_alloc, layer1_alloc);
          }

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

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e))
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector)
        {
          inode_reopen (inode);
          return inode;
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  cache_read (fs_device, inode->sector, &inode->data);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
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

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);

      /* Deallocate blocks if removed. */
      if (inode->removed)
        {
          #ifndef UNIXFFS
            free_map_release (inode->sector, 1);
            free_map_release (inode->data.start,
                            bytes_to_sectors (inode->data.length));
          #else
            free_map_release(inode->sector, 1);
          #endif
        }

      free (inode);
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode)
{
  ASSERT (inode != NULL);
  inode->removed = true;
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

  if (inode->deny_write_cnt)
    return 0;

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

  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode)
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode)
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  #ifndef UNIXFFS
    return inode->data.length;
  #else
    return inode->data->length;
  #endif
}
