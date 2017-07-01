# iwscan-xml — wireless network scanner with XML-formatted output

This repository contatins the results of the work I did in 2016 and which was 
mostly finished. So it may be usable... or may be not, depending of what you 
need.

## Problem

Main goal of this project is to implement scanning for wireless networks with 
machine-readable output format, primarily for use in Python scripts. Solutions
which parses `iwlist scan` output directly are not robust enough, because such
output primarily designed to be human-readable, and may be hard for a correct
and reliable machine parsing.

So it has been decided to modify `iwlist scan` source code to fit our purposes.

## Why not JSON?

* Main reason not to use JSON is its design limitation: it doesn't allow to 
  place multiple elements with identical names in the same object. This is a 
  serious   problem due to implementation specific of `iwlist scan`: it gets 
  scan results from kernel via socket and parses it with state machine. So, if 
  one of the data blocks ("events") contains e.g. list of frequencies, we cannot 
  know whether the next block will contain a continuation of this list, or some 
  other data. The most robust way is to close list after reaching end of data 
  block, but it may leads to the appearance of several lists with the same name, 
  containing fragments of one whole. This is not the problem for XML/XPath, but
  is a big problem for JSON.
* Second reason is possibility to easy transform XML with XSLT, e.g. to merge
  sets of "same" elements noted above into single ones (see
  `./stuff/iwscan-group-same.xslt`).
* Another reason is the existence of XPath query language, which allows to 
  easily search and extract various data from parsed XML tree (for *very simple*
  example see `./stuff/iwscan.py`).
* And some minor issues which mostly fixed in JSON5 (e.g. trailing commas) which
  however can bring a lot of pain with some "modern" JSON parsers.
  
TL;DR: JSON is great, but inappropriate for this particular task.

## How it works?

* `iwscan` is a simple console tool that works like `iwlist scan` excepting for
  output format (XML). See `./man/iwscan.{1,html}` for details.
* `iwscand` uses the same code (`iwscanlib.c` -> `libiwscan.so.0.1`), but works
  in a different manner: it starts as a daemon and waits for `SIGUSR1`. After
  receiving this signal, it performs a scan and dumps results to the given file
  (which can be ordinary file or FIFO). If you want to stop it, use `SIGTERM`.
  See `./man/iwscand.{1,html}` for details.

## Format

XML format of the scan results is described in `man/iwscan-xml-format.{7,html}`.

Also note that formatting must match to the scheme given in `./stuff/iwscan.xsd` 
and can be validated using `xmllint`.

## Installing

See `./INSTALL.md`.

## Licensing

Since this work is derivative from `wireless-tools` which licensed under
GPLv2, `iwscan-xml` also licensed under the same license.

## But there are modern tools like "iw"!

Yes. But this project uses obsolete `wireless-tools` as a base. This is a "fait 
accompli". Simply, do not use `iwscan-xml` if it doesn't fits your problem.

## Current status

Work was finished in the beginning of 2016, and I didn't touch the code since
that time. If you want to modify the code for your goal, **do not** consider 
yourself obliged to send me a pull requests, since I have not been working on
this project for a long time... Excepting for fixing bugs and vulnerabilities, 
because it definitely deserve attention.