تمرین گروهی ۳ - گزارش تغییرات نسبت به طراحی
======================

گروه 7
-----

>>‫نام و آدرس پست الکترونیکی اعضای گروه را در این قسمت بنویسید.

محمدسپهر پورقناد sepehrpourghannad@yahoo.com
سیدمحمدصادق کشاورزی mohammadsadeghkeshavarzi@yahoo.com
فرزام زهدی‌نسب farzamzohdi@gmail.com
مصطفی اوجاقی m.ojaghy@gmail.com

بافر کش
============

داده‌ساختار‌ها و توابع
---------------------

>>‫ در این قسمت تعریف هر یک از `struct` ها، اعضای `struct` ها، متغیرهای سراسری یا ایستا، `typedef` ها یا `enum` هایی که ایجاد کرده‌اید یا تغییر داده‌اید را‫ بنویسید و دلیل هر کدام را در حداکثر ۲۵ کلمه توضیح دهید.
```
lock cache_lock;

struct cache_block {
	lock lock;
	...
	int access_count;
}

void cache_flush ();
```

`cache_lock`: قفلی است که هنگام دسترسی به `cache` و `cache_hash` بکار می‌رود.
`lock`:برای هنگام سازی یک بلوک کش بکار می‌رود.
`access_count`: تعداد ریسه‌هایی که در حال حاضر در حال استفاده از بلوک کش هستند.
این ۳ مورد در طراحی اولیه وجود داشتند اما پیاده‌سازی بدون استفاده از آن‌ها صورت گرفت.
همچنین یک تابع`cache_flush ()` در پیاده‌سازی اضافه شد که زمانی کاربرد دارد که برای مثال سیستم `shut_down` شود. در چنین شرایطی نیاز است که قبل از خاموش شدن سیستم آنچه در کش باقی مانده است را منتقل کنیم.

زیرمسیرها
============

داده‌ساختار‌ها و توابع
----------------

>>‫ در این قسمت تعریف هر یک از `struct` ها، اعضای `struct` ها، متغیرهای سراسری‫ یا ایستا، `typedef` ها یا `enum` هایی که ایجاد کرده‌اید یا تغییر داده‌اید را بنویسید و‫ دلیل هر کدام را در حداکثر ۲۵ کلمه توضیح دهید.

```
struct thread {
	...
	block_sector_t cwd;
}

struct inode_disk {
	...
	bool is_dir;
	...
}
```



`cwd`: در طراحی اولیه از جنس `*dir` تعریف شده بود. `dir*` به چیزی در حافظه اشاره می‌کند و جهت اطمینان از اینکه مشکلی بوجود نیاید آن را به `block_sector_t` تغییر دادیم تا از روی دیسک خوانده و مطمئن باشیم.

`is_dir`: در طراحی اولیه بجای `inode_disk` در `dir_entry` قرار داشت. که یعنی در حافظه قرار داشت اما به این زمانی که پرونده را می‌بستیم همچنان به این اطلاعات نیاز داشتیم بنابراین آن را به `inode_disk` منتقل کردیم.

الگوریتم‌ها
-----------

>>‫ کد خود را برای طی کردن یک مسیر گرفته‌شده از کاربر را توضیح دهید.‫ آیا عبور از مسیرهای absolute و relative تفاوتی دارد؟
 در تابع `get_path()`، در ابتدا با توجه به مطلق یا نسبی بودن ورودی،‌ پوشه اولیه را مشخص می‌کنیم. سپس ورودی را `parse` کرده و ضمن توجه به `..`ها حرکت می‌کنیم. برای حرکت روبه داخل برای پوشه ها، از تابع `dir_lookup` استفاده می‌کنیم تا متوجه شویم که آیا پوشه‌ی فعلی موجود است یا خیر. در صورتی که موجود بود، یا `dir_open` ، پوشه جدید را بدست می‌اوردیم و `cwd` را متناسبا عوض می‌کنیم. در صورتی هم که بخواهیم احیانا به پوشه‌(های) پدر برویم، ابتدا از طریق `cwd` ، به ‍`inode` دسترسی پیدا می‌کنیم. سپس به `inode_disk` مربوطه و از آن طریق به  `block_sector_t parent_dir` می‌رسیم که همان محل ذخیره پوشه پدر در دیسک است. در نهایت هم با استفاده از `inode_open`  به `inode` پدر است. و می‌توانیم عملیات دلخواه خود را روی آن انجام دهیم.

تست‌ها
==========

تست اول
------
مورد دوم از ۳ مورد در این تست بررسی شده‌است. یعنی:
• در این سناریو توانایی حافظه نهان بافر خود را در ادغام و یکی کردن تغییرات بر روی یک sector را ارزیابی می‌کنید. به این صورت
که هر block حافظه دو شمارش‌گر cnt_read و cnt_write را نگه داری می‌کند. حال شروع به تولید و نوشتن یک فایل بزرگ به
صورت بایت به بایت کنید(حجم فایل تولید شده بیش از ۶۴ کیلوبایت باشد که دوبرابر اندازه بیشینه حافظه نهان بافر است). سپس
فایل را به صورت بایت به بایت بخوانید در صورت صحت کارکرد حافظه نهان بافر تعداد نوشتن بر روی دیسک باید بین ۱۲۸
 و ۱۹۲ مورد باشد (بدلیل اینکه ۶۴ کیلوبایت دارای ۱۲۸ block است و همچنین ممکن است تعدادی بلاک dirty هم در کش موجود باشد)
در صورتی که  تعداد نوشتن‌ها بین این دو عدد نباشد پیغام خطا چاپ شده و تست شکست می‌خورد.

merge-writes.output:
```
Copying tests/filesys/extended/merge-writes to scratch partition...
Copying tests/filesys/extended/tar to scratch partition...
qemu-system-i386 -device isa-debug-exit -hda /tmp/p5EKriH5zI.dsk -hdb tmp.dsk -m 4 -net none -nographic -monitor null
PiLo hda1
Loading............
Kernel command line: -q -f extract run merge-writes
Pintos booting with 3,968 kB RAM...
367 pages available in kernel pool.
367 pages available in user pool.
Calibrating timer...  207,667,200 loops/s.
hda: 1,008 sectors (504 kB), model "QM00001", serial "QEMU HARDDISK"
hda1: 200 sectors (100 kB), Pintos OS kernel (20)
hda2: 239 sectors (119 kB), Pintos scratch (22)
hdb: 5,040 sectors (2 MB), model "QM00002", serial "QEMU HARDDISK"
hdb1: 4,096 sectors (2 MB), Pintos file system (21)
filesys: using hdb1
scratch: using hda2
Formatting file system...done.
Boot complete.
Extracting ustar archive from scratch device into file system...
Putting 'merge-writes' into the file system...
Putting 'tar' into the file system...
Erasing ustar archive...
Executing 'merge-writes':
(merge-writes) begin
(merge-writes) create "cache"
(merge-writes) open "cache"
(merge-writes) end
merge-writes: exit(0)
Execution of 'merge-writes' complete.
Timer: 173 ticks
Thread: 49 idle ticks, 53 kernel ticks, 71 user ticks
hdb1 (filesys): 295 reads, 741 writes
hda2 (scratch): 238 reads, 2 writes
Console: 1077 characters output
Keyboard: 0 keys pressed
Exception: 0 page faults
Powering off...
```

merge-writes.result:
```
PASS
```


>   دو ایراد بالقوه و غیر بدیهی هسته را بیان کنید و نشان دهید که در صورت بروز این خطا تست شما چه خروجی خواهد داشت.
+ اگر هسته به کل قابلیت کش کردن را نداشته باشد در حدود ۶۵۵۳۶ عدد نوشتن انجام می‌شود و طبیعتا در تست شکست می‌خورد.
+ اگر هسته محتویات کش را به حافظه منتقل نکند، نوشتنی انجام نمی‌شود که این امر منجر به شکست در تست می‌شود.

تست دوم
------
مورد سوم از ۳ مورد در این تست بررسی گردیده‌. یعنی:
• در این سناریو توانایی حافظه نهان بافر خود را در نوشتن یک block کامل بدون اینکه نیاز باشد آن block را بخوانیم می‌آزماییم.
به عنوان مثال اگر شما ۴۰ کیلوبایت (۸۰ block) را در یک فایل می نویسید حافظه نهان بافر شما باید ۸۰  بار write_block
را صدا بزند اما هیچگاه تقاضایی برای خواندن از حافظه نداشته باشد با توجه به اینکه فایل به تازگی ایجاد شده است، اطلاعات آن در کش موجود است و برای دسترسی به آن نباید خواندن از دیسک انجام بگیرد.

درصورتی که برای تمامی ۸۰ عملیات نوشتن، عمل خواندن دیسک صورت نگیرد تست موفق خواهد بود و در غیر این صورت در تست شکست می‌خوریم.
 
dont-read.output:
```
Copying tests/filesys/extended/dont-read to scratch partition...
Copying tests/filesys/extended/tar to scratch partition...
qemu-system-i386 -device isa-debug-exit -hda /tmp/pcBUMujZJ_.dsk -hdb tmp.dsk -m 4 -net none -nographic -monitor null
PiLo hda1
Loading............
Kernel command line: -q -f extract run dont-read
Pintos booting with 3,968 kB RAM...
367 pages available in kernel pool.
367 pages available in user pool.
Calibrating timer...  383,795,200 loops/s.
hda: 1,008 sectors (504 kB), model "QM00001", serial "QEMU HARDDISK"
hda1: 200 sectors (100 kB), Pintos OS kernel (20)
hda2: 239 sectors (119 kB), Pintos scratch (22)
hdb: 5,040 sectors (2 MB), model "QM00002", serial "QEMU HARDDISK"
hdb1: 4,096 sectors (2 MB), Pintos file system (21)
filesys: using hdb1
scratch: using hda2
Formatting file system...done.
Boot complete.
Extracting ustar archive from scratch device into file system...
Putting 'dont-read' into the file system...
Putting 'tar' into the file system...
Erasing ustar archive...
Executing 'dont-read':
(dont-read) begin
(dont-read) create "cache"
(dont-read) open "cache"
(dont-read) end
dont-read: exit(0)
Execution of 'dont-read' complete.
Timer: 84 ticks
Thread: 14 idle ticks, 66 kernel ticks, 4 user ticks
hdb1 (filesys): 36 reads, 643 writes
hda2 (scratch): 238 reads, 2 writes
Console: 1047 characters output
Keyboard: 0 keys pressed
Exception: 0 page faults
Powering off...
```

dont-read.result:
```
PASS
```

>   دو ایراد بالقوه و غیر بدیهی هسته را بیان کنید و نشان دهید که در صورت بروز این خطا تست شما چه خروجی خواهد داشت.
+ اگر هسته به کل قابلیت کش کردن را نداشته باشد ۸۰ بار عمل خواندن دیسک انجام می‌شود که منجر به شکست در تست می‌شود.
+ اگر هسته برای هربار نوشتن در دیسک، محتویات دیسک را بخواند، ۸۰ بار عمل خواندن انجام می‌شود که باز هم منجر به شکست در تست می‌شود.

صحبت پایانی
-----
سیستم‌عامل pintos محدودیت ۱۴ کارکتر برای اسم فایل دارد و ساعتی سر کار بودیم. خدا داند که چطور این فیچر pintos را متوجه شدیم. 

کار تیمی
============
هماهنگی‌ها تقریبا مشابه با فاز‌های گذشته بودند و توضیحات دقیق‌تر از نحوه تعامل و کار تیمی در گزارشات گذشته موجود است.
+ طراحی:مشارکت تیمی
+ ماموریت یکم:مصطفی اوجاقی
+ ماموریت دوم:سپهر پورقنّاد + سیدمحمّدصادق کشاورزی
+ ماموریت سوم:سپهر پورقنّاد + سیدمحمّدصادق کشاورزی + مصطفی اوجاقی +‌ فرزام زهدی‌نسب
+ تست‌ها:مصطفی اوجاقی +‌ فرزام زهدی‌نسب
+ گزارش:فرزام زهدی‌نسب
