#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "devices/shutdown.h"
#include "userprog/pagedir.h"
#include "threads/vaddr.h"
#include "lib/string.h"

static void syscall_handler (struct intr_frame *);
static void fault_terminate (struct intr_frame *);
static bool is_valid_byte_addr (void *);
static bool is_valid_addr (void *, size_t);
static bool is_valid_str (char *);

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

  if (!is_valid_addr(args, 1 * sizeof(uint32_t)))
    fault_terminate(f);

  if (args[0] == SYS_EXIT)
    {
      if (!is_valid_addr(args, 2 * sizeof(uint32_t)))
        fault_terminate(f);
      f->eax = args[1];
      printf ("%s: exit(%d)\n", &thread_current ()->name, args[1]);
      thread_exit ();
    }
  else if (args[0] == SYS_WRITE)
    {
      if (!is_valid_addr(args, 2 * sizeof(uint32_t)))
        fault_terminate(f);
      int fd = args[1];
      if (fd == STDOUT_FILENO)
        {
          char *buffer = args[2];
          int size = args[3];
          for (int i = 0; i < size; i++)
            {
              printf("%c", buffer[i]);
            }
        }
    }
  else if (args[0] == SYS_PRACTICE)
    {
      if (!is_valid_addr(args, 2 * sizeof(uint32_t)))
        fault_terminate(f);
      f->eax = args[1] + 1;
    }
  else if (args[0] == SYS_HALT)
    {
      shutdown_power_off ();
    }
  else if (args[0] == SYS_EXEC)
    {
      if (!is_valid_addr(args, 2 * sizeof(uint32_t)) || !is_valid_str((char *) args[1]))
        fault_terminate(f);
        char *filename = args[1];
        tid_t tid = process_execute(filename);
        if (tid == TID_ERROR)
          {

          }
    }
  else if (args[0] == SYS_WAIT)
    {
      if (!is_valid_addr(args, 2 * sizeof(uint32_t)))
        {
          fault_terminate(f);
        }
      tid_t child_tid = args[1];
      struct thread *parent = thread_current();
      bool is_child = false;
      struct list_elem *e;
      for (e = list_begin (&parent->children); e != list_end (&parent->children);
       e = list_next (e))
        {
          struct thread_info *t = list_entry (e, struct thread_info, elem);
          if (t->tid == child_tid)
            {
              is_child = true;
              bool exited = t->exited;
              if (!exited)
                {
                  sema_down(t->sema);
                }
              break;
            }
        }
      if (!is_child)
        {
          printf("the tid is not belong to current threads' children");
          fault_terminate(f);
        }
    }
}

static void fault_terminate(struct intr_frame *f)
  {
    f->eax = -1;
    printf ("%s: exit(%d)\n", &thread_current()->name, -1);
    struct thread *t = thread_current();
    t->thread_info->exit_code = -1;
    thread_exit();
  }

static bool is_valid_byte_addr(void *addr)
{
    if (addr == NULL)
        return false;
    if (!is_user_vaddr(addr))
        return false;
    if (pagedir_get_page(thread_current()->pagedir, addr) == NULL)
        return false;
    return true;
}

// WARNING check last byte address

static bool is_valid_addr(void *addr, size_t size)
{
    return is_valid_byte_addr(addr) && is_valid_byte_addr(addr + size - 1);
}

static bool is_valid_str(char *str)
{
  if (is_valid_byte_addr((void *) str))
    {
      char *kernel_str = pagedir_get_page(thread_current()->pagedir, (void *) str);
      if (is_valid_byte_addr(str + strlen(kernel_str)))
        {
          return true;
        }
    }
  return false;
}
