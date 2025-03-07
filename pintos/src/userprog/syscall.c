#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "devices/shutdown.h"
#include "devices/block.h"
#include "userprog/pagedir.h"
#include "threads/vaddr.h"
#include "lib/string.h"
#include "threads/synch.h"
#include "filesys/filesys.h"

static void syscall_handler (struct intr_frame *);
static void fault_terminate (struct intr_frame *);
static bool is_valid_byte_addr (void *);
static bool is_valid_addr (void *, size_t);
static bool is_valid_str (char *);
struct thread_file *get_thread_file (int);

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED)
{
  uint32_t* args = ((uint32_t*) f->esp);

  /*
   * The following print statement, if uncommented, will print out the syscall
   * number whenever a process enters a system call. You might find it useful
   * when debugging. It will cause tests to fail, however, so you should not
   * include it in your final submission.
   */

  /* printf("System call number: %d\n", args[0]); */

  if (!is_valid_addr (args, 1 * sizeof (uint32_t)))
    fault_terminate (f);

  if (args[0] == SYS_EXIT)
    {
      if (!is_valid_addr (args, 2 * sizeof (uint32_t)))
        fault_terminate (f);
      f->eax = args[1];
      thread_current ()->thread_info->exit_code = args[1];
      printf ("%s: exit(%d)\n", &thread_current ()->name, args[1]);
      thread_exit ();
    }
  else if (args[0] == SYS_WRITE)
    {
      if (!is_valid_addr (args, 4 * sizeof (uint32_t)) || !is_valid_addr (args[2], args[3]))
        {
          fault_terminate (f);
        }
      int fd = args[1];

      if (fd == STDOUT_FILENO)
        {
          char *buffer = args[2];
          int size = args[3];
          for (int i = 0; i < size; i++)
            {
              printf ("%c", buffer[i]);
            }
        }
      else
        {
          void *buffer = args[2];
          off_t size = args[3];

          struct thread_file *tf = get_thread_file (fd);
          if (tf == NULL)
            {
              fault_terminate (f);
            }
          if (file_is_dir (tf->file)) 
            {
              f->eax = -1;
            }
          else
            {
              f->eax = file_write (tf->file, buffer, size);
            }
        }
    }
  else if (args[0] == SYS_PRACTICE)
    {
      if (!is_valid_addr (args, 2 * sizeof (uint32_t)))
        fault_terminate (f);
      f->eax = args[1] + 1;
    }
  else if (args[0] == SYS_HALT)
    {
      shutdown_power_off ();
    }
  else if (args[0] == SYS_EXEC)
    {
      if (!is_valid_addr (args, 2 * sizeof (uint32_t)) || !is_valid_str ((char *) args[1]))
        fault_terminate (f);

      char *file_name = args[1];
      struct file *file = NULL;
      bool file_exists;

      size_t name_len = strcspn (file_name, " ") + 1;
      char *name = malloc (name_len * sizeof (char));
      strlcpy (name, file_name, name_len);

      file = filesys_open (name);
      free (name);
      file_exists = (file != NULL);
      file_close (file);

      if (file_exists)
        {
          tid_t tid = process_execute (file_name);
          if (tid == TID_ERROR)
            {
              f->eax = -1;
            }
          else
            {
              struct thread_info *ti = get_thread_info (thread_current (), tid);
              if (ti == NULL)
                {
                  NOT_REACHED ();
                }
              else
                {
                  sema_down (&ti->load_sema);
                  if (ti->state == LOAD_FAILED)
                    {
                      f->eax = -1;
                    }
                  else
                    {
                      f->eax = tid;
                    }
                }
            }
        }
      else
        {
          f->eax = -1;
        }
    }
  else if (args[0] == SYS_WAIT)
    {
      if (!is_valid_addr (args, 2 * sizeof (uint32_t)))
        {
          fault_terminate (f);
        }
      tid_t child_tid = args[1];
      f->eax = process_wait (child_tid);
    }
  else if (args[0] == SYS_CREATE)
    {
      if (!is_valid_addr (args, 3 * sizeof (uint32_t)) || !is_valid_str (args[1]))
        {
          fault_terminate (f);
        }
      const char *name = args[1];
      off_t initial_size = args[2];
      f->eax = filesys_create (name, initial_size, false);
    }
  else if (args[0] == SYS_OPEN)
    {
      if (!is_valid_addr (args, 2 * sizeof (uint32_t)) || !is_valid_str (args[1]))
        {
          fault_terminate (f);
        }
      const char *name = args[1];
      struct file *opened_file = filesys_open (name);
      if (opened_file == NULL)
        {
          f->eax = -1;
        }
      else
        {
          f->eax = add_to_files (thread_current (), opened_file);
        }
    }
  else if (args[0] == SYS_REMOVE)
    {
      if (!is_valid_addr (args, 2 * sizeof (uint32_t)) || !is_valid_str (args[1]))
        {
          fault_terminate (f);
        }
      const char *name = args[1];
      f->eax = filesys_remove (name);
    }
  else if (args[0] == SYS_FILESIZE)
    {
      if (!is_valid_addr (args, 2 * sizeof (uint32_t)))
        {
          fault_terminate (f);
        }

      int fd = args[1];
      struct thread_file *tf = get_thread_file (fd);
      if (tf == NULL)
        {
          fault_terminate (f);
        }
      f->eax = file_length (tf->file);

    }
  else if (args[0] == SYS_READ)
    {
      if (!is_valid_addr (args, 4 * sizeof (uint32_t)) || !is_valid_addr (args[2], args[3]))
        {
          fault_terminate (f);
        }

      int fd = args[1];
      void *buffer = args[2];
      off_t size = args[3];

      struct thread_file *tf = get_thread_file (fd);
      if (tf == NULL)
        {
          fault_terminate (f);
        }
      f->eax = file_read (tf->file, buffer, size);

    }
  else if (args[0] == SYS_SEEK)
    {
      if (!is_valid_addr (args, 3 * sizeof (uint32_t)))
        {
          fault_terminate (f);
        }

      int fd = args[1];
      off_t position = args[2];
      struct thread_file *tf = get_thread_file (fd);
      if (tf == NULL)
        {
          fault_terminate (f);
        }
      file_seek (tf->file, position);

    }
  else if (args[0] == SYS_TELL)
    {
      if (!is_valid_addr (args, 2 * sizeof (uint32_t)))
        {
          fault_terminate (f);
        }

      int fd = args[1];
      struct thread_file *tf = get_thread_file (fd);
      if (tf == NULL)
        {
          fault_terminate (f);
        }
      f->eax = file_tell (tf->file);

    }
  else if (args[0] == SYS_CLOSE)
    {
      if (!is_valid_addr (args, 2 * sizeof (uint32_t)))
        {
          fault_terminate (f);
        }

      int fd = args[1];
      struct thread_file *tf = get_thread_file (fd);
      if (tf == NULL)
        {
          fault_terminate (f);
        }
      file_close (tf->file);
      list_remove (&tf->elem);
      free (tf);

    }
  else if (args[0] == SYS_INUMBER)
    {
      if (!is_valid_addr (args, 2 * sizeof (uint32_t)))
        {
          fault_terminate (f);
        }

      int fd = args[1];
      struct thread_file *tf = get_thread_file (fd);
      if (tf == NULL)
        {
          fault_terminate (f);
        }
      f->eax = (int) file_inumber (tf->file);

    }
  else if (args[0] == SYS_ISDIR)
    {
      if (!is_valid_addr (args, 2 * sizeof (uint32_t)))
        {
          fault_terminate (f);
        }

      int fd = args[1];
      struct thread_file *tf = get_thread_file (fd);
      if (tf == NULL)
        {
          fault_terminate (f);
        }
      f->eax = file_is_dir (tf->file);

    }
  else if (args[0] == SYS_READDIR)
    {
      if (!is_valid_addr (args, 3 * sizeof (uint32_t)) || !is_valid_str (args[2]))
        {
          fault_terminate (f);
        }

      int fd = args[1];
      struct thread_file *tf = get_thread_file (fd);
      if (tf == NULL)
        {
          fault_terminate (f);
        }
      if (!file_is_dir (tf->file))
        {
          f->eax = false;
        }
      else
        {
          f->eax = filesys_readdir (tf->file, (char *)args[2]);
        }

    }
  else if (args[0] == SYS_MKDIR)
    {
      if (!is_valid_addr (args, 2 * sizeof (uint32_t)) || !is_valid_str (args[1]))
        {
          fault_terminate (f);
        }
      const char *name = args[1];

      f->eax = filesys_create (name, 0, true);
    }
  else if (args[0] == SYS_CHDIR)
    {
      if (!is_valid_addr (args, 2 * sizeof (uint32_t)) || !is_valid_str (args[1]))
        {
          fault_terminate (f);
        }
      const char *name = args[1];
      block_sector_t cwd = filesys_chdir (name);
      if (cwd != -1) 
        {
          thread_current ()->cwd = cwd;
          f->eax = true;
        }
      else
        f->eax = false;
    }
  else if (args[0] == SYS_BLOCK_READS)
    {
      f->eax = get_block_reads (BLOCK_FILESYS);
    }
  else if (args[0] == SYS_BLOCK_WRITES)
    {
      f->eax = get_block_writes (BLOCK_FILESYS);
    }
}

static void fault_terminate (struct intr_frame *f)
  {
    f->eax = -1;
    printf ("%s: exit(%d)\n", &thread_current ()->name, -1);
    struct thread *t = thread_current ();
    t->thread_info->exit_code = -1;
    thread_exit ();
  }

static bool is_valid_byte_addr (void *addr)
{
    if (addr == NULL)
        return false;
    if (!is_user_vaddr (addr))
        return false;
    if (pagedir_get_page (thread_current ()->pagedir, addr) == NULL)
        return false;
    return true;
}

static bool is_valid_addr (void *addr, size_t size)
{
    return is_valid_byte_addr (addr) && is_valid_byte_addr (addr + size - 1);
}

static bool is_valid_str (char *str)
{
  if (is_valid_byte_addr ((void *) str))
    {
      char *kernel_str = pagedir_get_page (thread_current ()->pagedir, (void *) str);
      if (is_valid_byte_addr (str + strlen (kernel_str)))
        {
          return true;
        }
    }
  return false;
}

struct thread_file *
get_thread_file (int fd)
{
  struct thread *t = thread_current ();
  struct list_elem *e;
  for (e = list_begin (&t->files); e != list_end (&t->files);
      e = list_next (e))
      {
        struct thread_file *tf = list_entry (e, struct thread_file, elem);
        if (tf->fd == fd)
          return tf;
      }
  return NULL;
}
