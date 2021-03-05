تمرین گروهی ۱/۰ - آشنایی با pintos
======================

شماره گروه:
-----
> نام و آدرس پست الکترونیکی اعضای گروه را در این قسمت بنویسید.

نام و نام خانوادگی <example@example.com>

نام و نام خانوادگی <example@example.com> 

نام و نام خانوادگی <example@example.com> 

نام و نام خانوادگی <example@example.com> 

مقدمات
----------
> اگر نکات اضافه‌ای در مورد تمرین یا برای دستیاران آموزشی دارید در این قسمت بنویسید.


> لطفا در این قسمت تمامی منابعی (غیر از مستندات Pintos، اسلاید‌ها و دیگر منابع  درس) را که برای تمرین از آن‌ها استفاده کرده‌اید در این قسمت بنویسید.

آشنایی با pintos
============
>  در مستند تمرین گروهی ۱۹ سوال مطرح شده است. پاسخ آن ها را در زیر بنویسید.


## یافتن دستور معیوب

۱.
0xc0000008

۲.
0x8048757

۳.
08048754 <_start>: 
mov    0x24(%esp),%eax

۴.
void
_start (int argc, char *argv[])
{
  exit (main (argc, argv));
}


08048754 <_start>:
 8048754:	83 ec 1c             	sub    $0x1c,%esp
 8048757:	8b 44 24 24          	mov    0x24(%esp),%eax
 804875b:	89 44 24 04          	mov    %eax,0x4(%esp)
 804875f:	8b 44 24 20          	mov    0x20(%esp),%eax
 8048763:	89 04 24             	mov    %eax,(%esp)
 8048766:	e8 35 f9 ff ff       	call   80480a0 <main>
 804876b:	89 04 24             	mov    %eax,(%esp)
 804876e:	e8 49 1b 00 00       	call   804a2bc <exit>

Due to the calling convention, at first, arguments of the funcitons must be pushed to stack (in our
case is argc and argv). then desired function is called (main) and at the end exit function will be called. Note that by convention the rightmost argument is pushed first.
This instruction copies one of the caller's arguments into eax to push it onto the callee's stack as its argument.
Therefore, in our case, when it wants to push argv in stack, it crashes.

۵.
As mention above, it wants to access to its arguments (argv is intended in the crashed instruction) and since the argv has not been set properly it crashes.

## به سوی crash

۶.
pintos-debug: dumplist #0: 0xc000e000 
{
    tid = 1,
    status = THREAD_RUNNING,
    name = "main",
    '\000' <repeats 11 times>,
    stack = 0xc000edec <incomplete sequence \357>, 
    priority = 31, 
    allelem = {prev = 0xc0035910 <all_list>, next = 0xc0104020}, 
    elem = {prev = 0xc0035920 <ready_list>, next = 0xc0035928 <ready_list+8>}, 
    pagedir = 0x0, 
    magic = 3446325067
}
pintos-debug: dumplist #1: 0xc0104000 {
    tid = 2, 
    status = THREAD_BLOCKED, 
    name = "idle", 
    '\000' <repeats 11 times>, 
    stack = 0xc0104f34 "", 
    priority = 0, 
    allelem = {prev = 0xc000e020, next = 0xc0035918 <all_list+8>}, 
    elem = {prev = 0xc0035920 <ready_list>, next = 0xc0035928 <ready_list+8>}, 
    pagedir = 0x0, 
    magic = 3446325067
}

current thread is `main` in address `0xc000e000`.
other thread is `idle` which is blocked.

۷.

#0  process_execute (file_name=file_name@entry=0xc0007d50 "do-nothing") at ../../userprog/process.c:32
#1  0xc0020268 in run_task (argv=0xc00357cc <argv+12>) at ../../threads/init.c:288
#2  0xc0020921 in run_actions (argv=0xc00357cc <argv+12>) at ../../threads/init.c:340
#3  main () at ../../threads/init.c:133

#### run_task
```
/ Runs the task specified in ARGV[1]. /
static void
run_task (char **argv)
{
  const char *task = argv[1];

  printf ("Executing '%s':\n", task);
#ifdef USERPROG
  process_wait (process_execute (task));
#else
  run_test (task);
#endif
  printf ("Execution of '%s' complete.\n", task);
}
```

### run_actions
```
/* Executes all of the actions specified in ARGV[]
   up to the null pointer sentinel. */
static void
run_actions (char **argv)
{
  / An action. /
  struct action
    {
      char name;                       / Action name. */
      int argc;                         / # of args, including action name. /
      void (function) (char *argv);   / Function to execute action. /
    };

  / Table of supported actions. /
  static const struct action actions[] =
    {
      {"run", 2, run_task},
#ifdef FILESYS
      {"ls", 1, fsutil_ls},
      {"cat", 2, fsutil_cat},
      {"rm", 2, fsutil_rm},
      {"extract", 1, fsutil_extract},
      {"append", 2, fsutil_append},
#endif
      {NULL, 0, NULL},
    };

  while (*argv != NULL)
    {
      const struct action *a;
      int i;

      / Find action name. /
      for (a = actions; ; a++)
        if (a->name == NULL)
          PANIC ("unknown action `%s' (use -h for help)", *argv);
        else if (!strcmp (*argv, a->name))
          break;

      / Check for required arguments. /
      for (i = 1; i < a->argc; i++)
        if (argv[i] == NULL)
          PANIC ("action `%s' requires %d argument(s)", *argv, a->argc - 1);

      / Invoke action and advance. /
      a->function (argv);
      argv += a->argc;
    }

}
```
### main
```
int main (void) NO_RETURN;

/ Pintos main program. /
int
main (void)
{
char **argv;

/ Clear BSS. /
bss_init ();

/ Break command line into arguments and parse options. /
argv = read_command_line ();
argv = parse_options (argv);

/* Initialize ourselves as a thread so we can use locks,
then enable console locking. */
thread_init ();
console_init ();

/ Greet user. /
printf ("Pintos booting with %'"PRIu32" kB RAM...\n",
init_ram_pages * PGSIZE / 1024);

/ Initialize memory system. /
palloc_init (user_page_limit);
malloc_init ();
paging_init ();

/ Segmentation. /
#ifdef USERPROG
tss_init ();
gdt_init ();
#endif

/ Initialize interrupt handlers. /
intr_init ();
timer_init ();
kbd_init ();
input_init ();
#ifdef USERPROG
exception_init ();
syscall_init ();
#endif

/ Start thread scheduler and enable interrupts. /
thread_start ();
serial_init_queue ();
timer_calibrate ();

#ifdef FILESYS
/ Initialize file system. /
ide_init ();
locate_block_devices ();
filesys_init (format_filesys);
#endif

printf ("Boot complete.\n");

/ Run actions specified on kernel command line. /
run_actions (argv);

/ Finish up. /
shutdown ();
thread_exit ();
}
```

۸.

pintos-debug: dumplist #0: 0xc000e000 
{
    tid = 1,
    status = THREAD_BLOCKED, 
    name = "main", 
    '\000' <repeats 11 times>, 
    stack = 0xc000eeac "\001", 
    priority = 31, 
    allelem = {prev = 0xc0035910 <all_list>, next= 0xc0104020}, 
    elem = {prev = 0xc0037314 <temporary+4>, next = 0xc003731c <temporary+12>}, 
    pagedir = 0x0, 
    magic = 3446325067
}
pintos-debug: dumplist #1: 0xc0104000 
{
    tid = 2, 
    status = THREAD_BLOCKED, 
    name = "idle", 
    '\000' <repeats 11 times>, 
    stack = 0xc0104f34 "", 
    priority = 0, 
    allelem = {prev = 0xc000e020, next = 0xc010a020}, 
    elem = {prev = 0xc0035920 <ready_list>, next = 0xc0035928 <ready_list+8>}, 
    pagedir = 0x0, 
    magic = 3446325067
}
pintos-debug: dumplist #2: 0xc010a000 
{
    tid = 3, 
    status = THREAD_RUNNING, 
    name = "do-nothing\000\000\000\000\000", 
    stack = 0xc010afd4 "", 
    priority = 31, 
    allelem = {prev = 0xc0104020, next = 0xc0035918 <all_list+8>}, 
    elem = {prev = 0xc0035920 <ready_list>, next = 0xc0035928 <ready_list+8>}, 
    pagedir = 0x0, 
    magic = 3446325067
}

existing threads are `main`, `idle`, `do-nothing\000\000\000\000\000`.

۹.
```
tid_t
process_execute (const char *file_name)
{
  char *fn_copy;
  tid_t tid;

  sema_init (&temporary, 0);
  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  fn_copy = palloc_get_page (0);
  if (fn_copy == NULL)
    return TID_ERROR;
  strlcpy (fn_copy, file_name, PGSIZE);

  /* Create a new thread to execute FILE_NAME. */
  tid = thread_create (file_name, PRI_DEFAULT, start_process, fn_copy);
  if (tid == TID_ERROR)
    palloc_free_page (fn_copy);
  return tid;
}
```

In which `tid = thread_create (file_name, PRI_DEFAULT, start_process, fn_copy);` line is corresponding line for creating `start_process` thread.

۱۰.
before load:
{
    edi = 0x0,
    esi = 0x0, 
    ebp = 0x0, 
    esp_dummy = 0x0, 
    ebx = 0x0, 
    edx = 0x0, 
    ecx = 0x0, 
    eax = 0x0, 
    gs = 0x23, 
    fs = 0x23, 
    es = 0x23, 
    ds = 0x23, 
    vec_no = 0x0, 
    error_code = 0x0, 
    frame_pointer = 0x0, 
    eip = 0x0, 
    cs = 0x1b, 
    eflags = 0x202, 
    esp = 0x0, 
    ss = 0x23
}

after load:
{
    edi = 0x0, 
    esi = 0x0, 
    ebp = 0x0, 
    esp_dummy = 0x0, 
    ebx = 0x0, 
    edx = 0x0, 
    ecx = 0x0, 
    eax = 0x0, 
    gs = 0x23, 
    fs = 0x23, 
    es = 0x23, 
    ds = 0x23, 
    vec_no = 0x0, 
    error_code = 0x0, 
    frame_pointer = 0x0, 
    eip = 0x8048754, 
    cs = 0x1b, 
    eflags = 0x202, 
    esp = 0xc0000000, 
    ss = 0x23
}

۱۱.

To start the user process, we need to set its segments. At the begining of the start_process function, it sets the interrupt frame in which the corresponding registers and frames for starting the user process is set. Afterward, it calls a function (`intr_exit`) that exits from an interrupt. In fact, it is just utilized to set the frames for the user space. Thus after finishing the funcition we will be in the user space.

۱۲.

eax            0x0      0
ecx            0x0      0
edx            0x0      0
ebx            0x0      0
esp            0xc0000000       0xc0000000
ebp            0x0      0x0
esi            0x0      0
edi            0x0      0
eip            0x8048754        0x8048754
eflags         0x202    [ IF ]
cs             0x1b     27
ss             0x23     35
ds             0x23     35
es             0x23     35
fs             0x23     35
gs             0x23     35

The value of registers are same!

۱۳.

#0  _start (argc=<unavailable>, argv=<unavailable>) at ../../lib/user/entry.c:9

## دیباگ

۱۴.

We subtract the `%esp` by 20 at the end of `load` function.
We use -20 for aligning the `%esp`. At first note that due to the convention, the `%esp` must be 16-byte-aligned before `call`. Therefore after executing `call` the `%esp` remainder mod `16` will be `-4`. According to the mentioned assembly code in `#1` the reason for the crash is that the code wants to access a part of memory which is not allocated for user space (i.e above 0xc0000000 that is the default physical base address for user space). Hence we set `%esp` according to function arguments derived from filename. In our case, since we just have 2 arguments i.e `argc` and `argv`, 20 is sufficient.


۱۵.

As just mentioned above the `%esp` mod `16` must be -4.

۱۶.
0xbfffffa8:     0x00000001      0x000000a2

۱۷.
args[0] = 1 -> 0x00000001
args[1] = 162 -> 0x000000a2

۱۸.
The corresponding call to   `sema_down (&temporary);` is in process_wait in `process.c`.
When `process_exit` is finished, it releases this semaphore and wakes up `process_wait` which has been waiting for the process to finish and returns its exit status.

۱۹.

pintos-debug: dumplist #0: 0xc000e000 
{
  tid = 1, 
  status = THREAD_RUNNING, 
  name = "main", '\000' <repeats 11 times>, 
  stack = 0xc000eeac "\001",
  priority = 31, 
  allelem = {prev = 0xc0035910 <all_list>, next = 0xc0104020}, 
  elem = {prev = 0xc0035920 <ready_list>, next = 0xc0035928 <ready_list+8>}, 
  pagedir = 0x0, 
  magic = 3446325067
}

pintos-debug: dumplist #1: 0xc0104000 
{
  tid = 2, 
  status = THREAD_BLOCKED, 
  name = "idle", 
  '\000' <repeats 11 times>, 
  stack = 0xc0104f34 "", 
  priority = 0, 
  allelem = {prev = 0xc000e020, next = 0xc0035918 <all_list+8>}, 
  elem = {prev = 0xc0035920 <ready_list>, 
  next = 0xc0035928 <ready_list+8>}, 
  pagedir = 0x0, 
  magic = 3446325067
}

