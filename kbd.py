#!/usr/bin/python3

import sys
import json

if len(sys.argv) >= 2:
    x = json.load(open(sys.argv[1]))
else:
    x = json.load(sys.stdin)

othermap=[7,9,15,18,23,27,31,36,39,45,47,54,55,56,57,58,59,60,61,62,63]
others=["_Backspace", "_Up", "_Back", "_Shift", "_Left", "_PageUp", "_Escape",
        "_Down", "_Home", "_Symbols", "_Ctrl", "_PageDown", "_Alt", "_Space",
        "_Forward", "_Right", "_Return", "_End", "_Tab", "_Del", "_Numbers"]


primap=[1,2,4,8,16,32,
        3,6,24,48,
        5,40,
        11,19,35,
        14,22,38,
        25,26,28,
        49,50,52,
        13,29,21,53,37,
        41,43,42,46,44]
puncmap=[17,34,12,10,20,33,30,51]
y={}

def mapval(d, i):
    try:
        ri = primap.index(i)
        return d['primary'][ri]
    except ValueError:
        pass
    try:
        ri = puncmap.index(i)
        return d['punctuation'][ri]
    except ValueError:
        pass
    try:
        ri = othermap.index(i)
        return others[ri]
    except ValueError:
        pass
    return ''

def cstring(s):
    return '"' + s.replace('\\', '\\\\').replace('"', '\\"').replace('\n', '\\n') + '"'

print('struct keymap map = {')

for k in ['lowercase','uppercase','capslock','numbers','symbols']:
    print('\t.', end='')
    print(k, '=', '{', end='')
    print(', '.join((cstring(mapval(x[k], i)) for i in range(64))), '},', sep='')

print('};')

#print(y)
