#!/usr/bin/env python2

import subprocess
import lxml.etree as etree

# Run iwscan
# Assume that it installed with PREFIX=./test and has CAP_NET_ADMIN
p = subprocess.Popen(['./test/bin/iwscan', 'wlan0'], stdout=subprocess.PIPE);

# Wait for scan to finish, then get and parse XML
tr = etree.fromstringlist(p.communicate()[0])

# Sample search: print all MACs of APs which use WPA 2
print [x.text for x in tr.xpath('.//WPA[@type="2"]/../Address')]

