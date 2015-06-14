#!/usr/bin/env python

def cp866_to_utf8(c):
    c = chr(i)
    u = c.decode('cp866')
    v = u.encode('utf8')
    x = "0x%X" % (ord(v[0]) << 8 | ord(v[1]))
    if u.isalpha():
	return x

print "#ifndef __CP866_H_"
print "#define __CP866_H_"
print "uint16_t cp866[] = {"
for i in range(128, 256):
    x = cp866_to_utf8(i)
    if x != None:
	print "%s," % x, "0x%X," % i
print "};"
print "#endif"
