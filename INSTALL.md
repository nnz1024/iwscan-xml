# 1. Prerequisites

You need dynamically linked `libiw` from wireless-tools >= 30-pre9
(more older versions may work, but not tested, because 30-pre9
was released roughly 7 years ago, in 2009). You can get it on 

http://www.labs.hpe.com/personal/Jean_Tourrilhes/Linux/Tools.html

I.e. you must have `libiw.so.XX` (where XX>=30) somewhere in `[/usr]/lib`,
and corresponding `iwlib.h` and `wireless.h` somewhere in `/usr/include`.

Also don't forget about gcc, make and libc headers.

For correct and relatively secure operation of these tools, it is recommended
to have `setcap` utility from libcap2. You can get it from 

https://sites.google.com/site/fullycapable/

`setcap` allows you to run `iwscan` and `iwscand` under non-privileged user
(this utility required only while install stage).

**TL;DR:**
For Debian and derivatives, 
```
    # apt-get install libiw30 libiw-dev libcap2-bin build-essential
```
must be sufficient.

## 1.a. Recompiling man pages

Distribution must include pre-compiled manual pages in `./man` directory, as 
well as their sources (`./man/*.xml`). If you want to re-compile manual pages, 
you need `libxml2` and `xsltproc` from 

http://xmlsoft.org/

http://xmlsoft.org/xslt/

and the Docbook XSL stylesheet distribution (`docbook-xsl`) from 

http://wiki.docbook.org/DocBookXslStylesheets

For Debian and derivatives,
```
    # apt-get libxml2-utils xsltproc docbook-xsl
```
must be sufficient.

To recompile man pages, use
```
    cd ./man && make update-man
```

## 1.b. Additional software

Note that above-mentioned `libxml2` and `xsltproc` may be helpful also after 
installation, i.e. when using `iwscan`. `xmllint` allows you to check generated 
XML for consistency, while `xsltproc` allows you to transform generated XML 
in according to certain rules.

Also, if you plan to use generated XML in your Python scripts, it is recommended
to consider using `lxml`, a Python binding for `libxml2`. See

http://lxml.de/

for details.

# 2. Compilation

Just say 
``` 
    make
```

## 2.a. Compilation troubleshooting

* If you got an error like
  ```
        iwscan.h:17:21: fatal error: iwlib.h: No such file or directory
  ```
  check for libiw headers (see above).

* If error looks like
  ```
        /usr/bin/ld: cannot find -liw
  ```
  check for libiw.so.XX (see above).

# 3. Installation

Just say
```
    # make install
```
By default, it installs everything in `/usr/local`.

For installing into another location, use `PREFIX` variable, i.e.
```
    # make install PREFIX=./sandbox
```
If you want to run `iwscan` and `iwscand` without root privileges 
(recommended), you need to run `make install` under root, which allows
`setcap` utility (see above) to grant POSIX capability `CAP_NET_ADMIN`
(required for scanning) to our binaries.

If you install binaries with `setcap` into sandbox, it is recommended
to symlink or copy `libiwscan.so` into `/usr/lib` or `/usr/local/lib`,
because `LD_PRELOAD`-ing for binaries with capabilities isn't allowed.
E.g.
```    
    # cp -H path/to/sandbox/lib/libiwscan.so.0.1 /usr/lib
```
or
```
    # ln -s /full/path/to/sandbox/lib/libiwscan.so.0.1 /usr/lib
```
As an alternative, you can add `/full/path/to/sandbox/lib` into your 
`/etc/ld.so.conf` file, and then run `ldconfig` under root.

## 3.a. Important note

Depends on having `setcap` and using root for `make install`, you may get
one of two **different locations of binary files**.

* You have `setcap` in root's PATH and run `make install` as root.
  In this scenario, `iwscan` and `iwscand` will get required capabilities
  and may run under ordinary user, thus they will be installed into
  `$(PREFIX)/bin`.

* If you don't have `setcap` or run `make install` without root privileges,
  you hence cannot simply run `iwscan` and `iwscand` without root, thus
  they will be installed into `$(PREFIX)/sbin`.

Make will say you about that, e.g.
```
    *** setcap found. You probably can run iwscan and iwscand as user. ***
    *** Installing binaries into /usr/local/bin ***
```
or
```
    *** setcap not found. You can run iwscan and iwscand only as root. ***
    *** Installing binaries into /usr/local/sbin ***
```
Be careful!

BTW, in both cases, manual pages for `iwscan` and `iwscand` will be placed 
in section 1.