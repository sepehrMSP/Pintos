#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"

#define BLOCK_SIZE 512
#define TEST_SIZE (80 * BLOCK_SIZE)

void
test_main (void) 
{
  const char *file_name = "cache";

  int fd;
  size_t ofs;
  char zero[BLOCK_SIZE] = {0,};

  CHECK (create (file_name, TEST_SIZE), "create \"%s\"", file_name);
  CHECK ((fd = open (file_name)) > 1, "open \"%s\"", file_name);

  seek (fd, 0);
  int initial_block_reads = block_reads ();
  for (ofs = 0; ofs < TEST_SIZE; ofs+=BLOCK_SIZE)
    {
      int bytes_write = write (fd, &zero, BLOCK_SIZE);
      if (bytes_write != BLOCK_SIZE)
        fail ("fail to write one byte at ofs=%d", ofs);
    }
  
  int diff_block_reads = block_reads () - initial_block_reads;
  if (diff_block_reads > 0)
    fail ("kernel should not read any sectors, total_block_reads=%d",diff_block_reads);
  close (fd);
}
