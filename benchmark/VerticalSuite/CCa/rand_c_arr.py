#!/usr/bin/python

import random

len=2048

print "int randArr[" + str(len) + "] = { "

for i in range(0,len):
  print str(int(i)%2) + ", ",
  if i % 32 == 31:
    print ""

print "}"

