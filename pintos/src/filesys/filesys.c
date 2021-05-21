#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "threads/thread.h"

#define DIRS_LIMIT 1024

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);

bool
path_is_relative (const char* name)
{
  if (name != NULL){
    if (name[0] == '/')
      {
        return false;
      }
  }
  return true;
}

int
parse_dir (const char *dir_name, tok_t *dirs)
{
  size_t dir_counter = 0;
  char *rest;
  char *tok = strtok_r (dir_name, "/", &rest);

  while (tok != NULL)
    {
      dirs[dir_counter] = tok;
      dir_counter++;
      if (dir_counter == DIRS_LIMIT)
        {
          ASSERT (0);
          break;
        }
      tok = strtok_r (NULL, "/", &rest);
    }
  dirs[dir_counter] = NULL;
  return dir_counter;
}

/*
check if the name is match with this regex or not '/+'
*/
bool
is_root (const char *name)
{
  if (name[0] == '\0')
    return false;

  tok_t *dirs = malloc (sizeof(tok_t) * DIRS_LIMIT);
  char *namecpy = malloc (strlen (name) + 1);
  strlcpy (namecpy, name, strlen (name) + 1);
  int dirc = parse_dir (namecpy, dirs);
  free (namecpy);
  free (dirs);
  if (dirc == 0)
    {
      return true; 
    }
  return false;
}

struct dir *
get_path (const char *name, bool check_last, char* file_name)
{
  struct dir *cur_dir;
  if (path_is_relative(name))
    {
      struct inode *cwd_inode = inode_open (thread_current ()->cwd);
      decrement_inode_open_cnt (cwd_inode);
      cur_dir = dir_open (cwd_inode);
    }
  else
    cur_dir = dir_open_root ();

  // WARNING : len name may be 0 (probably has been checked in is_valid_str)
  tok_t *dirs = malloc(sizeof(tok_t) * DIRS_LIMIT);
  char *namecpy = malloc (strlen (name) + 1);
  strlcpy (namecpy, name, strlen (name) + 1);
  int dirc = parse_dir (namecpy, dirs);
  if (dirc == 0)
    {
      free(dirs);
      free(namecpy);
      return NULL;
    }

  if (!check_last)
    {
      dirc --;
    }
  for (int i = 0; i < dirc; i++)
    {
      if (!strcmp (dirs[i], ".."))
        {
          block_sector_t parent_dir_sector = get_dir_parent_sector(cur_dir);
          struct inode *parent_dir_inode = inode_open (parent_dir_sector);
          decrement_inode_open_cnt (parent_dir_inode);
          cur_dir = dir_open (parent_dir_inode);
        }
      else if (strcmp (dirs[i], "."))
        {
          struct inode *inode = NULL;
          if (!dir_lookup (cur_dir, dirs[i], &inode))
            {
              free(dirs);
              free(namecpy);
              return NULL;
            }
          if (!inode_is_dir (inode))  
            {
              free(dirs);
              free(namecpy);
              return NULL;
            }
          cur_dir = dir_open (inode);
          decrement_inode_open_cnt (inode);
        }
    }
  if (!check_last && file_name != NULL)
    {
      if (strlen(dirs[dirc]) > NAME_MAX)
        {
          free(dirs);
          free(namecpy);
          return NULL;
        }
      strlcpy (file_name, dirs[dirc], NAME_MAX + 1);
    }
  free(dirs);
  free(namecpy);
  return cur_dir;
}

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format)
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  free_map_init ();

  if (format)
    do_format ();

  free_map_open ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void)
{
  free_map_close ();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size, bool is_dir)
{
  block_sector_t inode_sector = 0;
  char *file_name = malloc(NAME_MAX + 1);
  // struct dir *dir = dir_open_root ();
  struct dir *dir = get_path (name, false, file_name);
  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size, is_dir)
                  && dir_add (dir, file_name, inode_sector));
  if (!success && inode_sector != 0)
    free_map_release (inode_sector, 1);
  dir_close (dir);
  free(file_name);
  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  if (is_root(name))
    {
      struct dir *root = dir_open_root ();
      struct inode *inode = dir_get_inode (root);
      // dir_close (root);
      decrement_inode_open_cnt (inode);
      return file_open (inode);
    }
  char *file_name = malloc(NAME_MAX + 1);
  struct dir *dir = get_path (name, false, file_name);
  struct inode *inode = NULL;

  if (dir != NULL)
    {
      if (!strcmp (name, "."))
        {
          inode = dir_get_inode (dir);
          increment_inode_open_cnt (inode);
          free (file_name);
          return file_open (inode);
        }
      else
        dir_lookup (dir, file_name, &inode);
    }
  free (file_name);
  dir_close (dir);

  return file_open (inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name)
{
  struct dir *dir;
  char *file_name = malloc (NAME_MAX + 1);

  if (is_root (name)) 
    {
      return false;
    }
  else
    {
      dir = get_path (name, false, file_name);
    }
  bool success = dir != NULL && dir_remove (dir, file_name);
  dir_close (dir);
  free (file_name);

  return success;
}

block_sector_t
filesys_chdir (const char *name)
{
  if (is_root (name))
    {
      struct dir *root = dir_open_root ();
      block_sector_t bst = get_dir_sector (root);
      dir_close (root);
      return bst;
    }
  struct dir *dir = get_path (name, true, NULL);
  if (dir == NULL)
    return -1;

  block_sector_t res = get_dir_sector (dir);
  dir_close (dir);
  return res;
}

/*This funciton get the file corresponding to the desire directory 
  and update the pos after reading process
*/
bool
filesys_readdir (struct file *file, char name[NAME_MAX + 1])
{
  struct dir *dir = get_directory (file);
  bool res = dir_readdir (dir, name);
  set_file_pos(file, get_directory_pos (dir), res);
  return res;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}
