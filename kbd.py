#!/usr/bin/python2

from __future__ import print_function, unicode_literals
import sys
import json
import subprocess

if len(sys.argv) >= 2:
    x = json.load(open(sys.argv[1]))
else:
    x = json.load(sys.stdin)

othermap=[7,9,15,18,23,27,31,36,39,45,47,54,55,56,57,58,59,60,61,62,63]
others=["_BackSpace", "_Up", "*Left", "+Shift", "_Left", "_Prior", "_Escape",
        "_Down", "_Home", "/SYMBOLS", "+Control", "_Next", "+Alt", "_space",
        "*Right", "_Right", "_Return", "_End", "_Tab", "_Delete", "/NUMBERS"]

#syms = {
#        '' : 'XK_VoidSymbol',
#        '_': 'XK_underscore',
#        ',': 'XK_comma',
#        "'": 'XK_apostrophe',
#        '!': 'XK_exclam',
#        '-': 'XK_minus',
#        '/': 'XK_slash',
#        '?': 'XK_question',
#        '.': 'XK_period',
#        '\\':'XK_backslash',
#        '(': 'XK_parenleft',
#        ')': 'XK_parenright',
#        '*': 'XK_asterisk',
#        '%': 'XK_percent',
#        '|': 'XK_bar',
#        '[': 'XK_bracketleft',
#        ']': 'XK_bracketright',
#        '$': 'XK_dollar',
#        '=': 'XK_equal',
#        ';': 'XK_semicolon',
#        '<': 'XK_less',
#        '>': 'XK_greater',
#}

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

def keysym(d, i):
    orig = mapval(d, i)
    if orig == '':
        return 'NONE', 'NoSymbol'
#    try:
#        return 'PRESS', syms[orig]
#    except KeyError:
#        pass
    if len(orig) == 1:
        return 'PRESS', subprocess.check_output(['./symname', 'U00' + hex(ord(orig))[2:]])
    if len(orig) == 1 and 'a' <= orig <= 'z':
        return 'PRESS', 'XK_' + orig
    if orig[0] == '_':
        return 'PRESS', 'XK' + orig
    if orig[0] == '+':
        return 'MOD', 'XK_' + orig[1:] + '_L'
    if orig[0] == '/':
        return 'MAP', orig[1:]
    return 'NONE', 'NoSymbol'
    #return 'MACRO', '"' + orig + '"'

def cstring(s):
    return '"' + s.replace('\\', '\\\\').replace('"', '\\"').replace('\n', '\\n') + '"'

print('union keymap map[][64] = {')

#for k in ['lowercase','uppercase','capslock','numbers','symbols']:
for k in ['lowercase','numbers','symbols']:
    print('\t[', k.upper(), '] = {', sep='')
    for i in range(64):
        print('\t\t{', ', '.join(keysym(x[k], i)), '},', sep='')
    print('\t},')
    #print(',\n\t\t'.join((', '.join(keysym(x[k], i)) for i in range(64))), '\n\t},', sep='')

print('};')
