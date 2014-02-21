This is a minor derivation of iconv2.  The only advantage of this package
is that programs linked against the provided libraries do not require that the
version of OS/2 is new enough to support UCONV.DLL.  All they require is
that iconv.dll with 3 export points iconv() iconv_close() iconv_open().
While the supplied DLL works on Warp 3 after fp32, and Warp 4 after fp4, one
can easily compile such a DLL (from POSIX sources) which would work on
any version of OS/2 - if a need arises.

The disadvantage of this package is that it is specific to EMX -Zmtd
executables (but it comes with manpages too! ;-).

[This is a second release; the first was not linked with -Zmt, and used the
code of iconv2-0_2_1.zip which was very buggy.  This release passes the
tests of Perl module Text::Iconv v1.4.]

=========================================================  HOWTO INSTALL

Copy ./include/* to a directory on your C_INCLUDE_PATH environment variable,
./lib/* to LIBRARY_PATH, ./man/* to MANPATH; put ./dll/* to LIBPATH.

To get information in HTML, google for "iconv.3".  E.g.,
  http://inferno.slug.org/man-pages-1.53/man3/iconv.html

To get autoritative information, you need to look for a key on a certain
server.  Google for

  http://www.opengroup.org/onlinepubs   functions/iconv.html

Enjoy, Ilya Zakharevich

=========================================================  HOWTO BUILD

gcc -Ii:/toolkit/h -D__ST_MT_ERRNO__ -c -Zomf -Zmtd -I. -pipe -O2 -s -malign-loops=2 -malign-jumps=2 -malign-functions=2 -Zdll my_iconv.c
gcc -O -s -Zdll -Zmtd -Zomf my_iconv.obj -o iconv.dll iconv.def i:/toolkit/lib/libconv.lib
lxlite *.dll
emximp -o iconv.lib iconv.def
emximp -o iconv.a   iconv.def
