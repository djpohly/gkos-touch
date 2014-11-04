#!/usr/bin/python2

from __future__ import print_function, unicode_literals
import sys
import json
import subprocess

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
        return 'NONE', '.sym = NoSymbol'
#    try:
#        return 'PRESS', syms[orig]
#    except KeyError:
#        pass
    if len(orig) == 1:
        return 'PRESS', ('.sym = ' +
            subprocess.check_output(['./symname', 'U00' + hex(ord(orig))[2:]]))
    if orig == '_Ins':
        return 'PRESS', '.sym = XK_Insert'
    if orig[0] == '_':
        return 'PRESS', '.sym = XK' + orig
    if orig[0] == '+':
        return 'MOD', '.sym = XK_' + orig[1:] + '_L'
    if orig[0] == '/':
        return 'MAP', '.map = ' + orig[1:]
    return 'NONE', '.sym = NoSymbol'
    #return 'MACRO', '"' + orig + '"'

namemap={
        'lowercase': 'LETTERS',
        'numbers': 'NUMBERS',
        'symbols': 'SYMBOLS',
}

print('struct keymap_entry ' + sys.argv[1] + '[][64] = {')

for k in namemap.keys():
    print('\t[', namemap[k], '] = {', sep='')
    for i in range(64):
        print('\t\t{.type=', ', .val={'.join(keysym(x[k], i)), '}},', sep='')
    print('\t},')

print('};')
