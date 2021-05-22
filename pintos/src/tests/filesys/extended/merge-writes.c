#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"

#define TEST_SIZE (128 * 512)

void
test_main (void) 
{
  const char *file_name = "cache";

  int fd;
  size_t ofs;
  char zero = 0;

  CHECK (create (file_name, TEST_SIZE), "create \"%s\"", file_name);
  CHECK ((fd = open (file_name)) > 1, "open \"%s\"", file_name);

  seek (fd, 0);
  int initial_block_writes = block_writes ();
  for (ofs = 0; ofs < TEST_SIZE; ofs++)
    {
      int bytes_write = write (fd, &zero, 1);
      if (bytes_write != 1)
        fail ("fail to write one byte at ofs=%d", ofs);
    }
  seek (fd, 0);
  for (ofs = 0; ofs < TEST_SIZE; ofs++)
    {
      char read_data;
      int bytes_read = read (fd, &read_data, 1);
      if (bytes_read != 1)
        fail ("fail to write one byte at ofs=%d", ofs);
      if (read_data != zero)
        fail ("data mismatch at %d", ofs);
    }
  
  int diff_block_writes = block_writes () - initial_block_writes;
  if (diff_block_writes > 192 || diff_block_writes < 128)
    fail ("total writes should be in [128,192], total_block_writes=%d",diff_block_writes);
  close (fd);
}
