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

۷.

۸.

۹.

۱۰.

۱۱.

۱۲.

۱۳.


## دیباگ

۱۴.

۱۵.

۱۶.

۱۷.

۱۸.

۱۹.
