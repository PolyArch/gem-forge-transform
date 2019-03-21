#!/bin/python

import random

len=2048

print "int randArr[" + str(len) + "] = { "

for i in range(0,len):
  print str(random.randint(0,1)) + ", ",
  if i % 32 == 31:
    print ""

print "}"

