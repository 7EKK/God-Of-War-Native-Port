"""Print the R5900 disassembly of a recompiled function (from the comments in port/output).
usage: dis.py ADDR   (ADDR = any address inside the function, hex)"""
import os, re, sys
a = int(sys.argv[1], 16)
best = None
for name in os.listdir('port/output'):
    m = re.match(r'(?:sub|entry)_([0-9A-F]{8})_', name)
    if m and int(m.group(1), 16) <= a and (best is None or int(m.group(1), 16) > best[0]):
        best = (int(m.group(1), 16), name)
for line in open(os.path.join('port/output', best[1])):
    m = re.match(r'\s*// (0x[0-9a-f]+): 0x[0-9a-f]+\s+(.*)', line)
    if m:
        print(('=> ' if int(m.group(1), 16) == a else '   ') + m.group(1) + ': ' + m.group(2))
print('function', best[1])
