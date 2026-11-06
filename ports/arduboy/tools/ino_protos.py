#!/usr/bin/env python3
"""Write an Arduino sketch to a C++ file the way the Arduino IDE does: a block of function
prototypes goes in before the first function, so functions can be called before they are defined.
The sketch text is otherwise untouched.

    ino_protos.py sketch.ino out.cpp
"""
import re
import sys

FUNC = re.compile(
    r'^(?P<ret>(?:static\s+|inline\s+|const\s+|unsigned\s+)*[A-Za-z_][\w:<>]*[\s\*&]+)(?P<name>[A-Za-z_]\w*)\s*\((?P<args>[^;{}()]*)\)\s*(?:\{|$)',
    re.M)
SKIP = {'if', 'for', 'while', 'switch', 'return', 'else', 'sizeof'}


def main(src, dst):
    text = open(src).read()
    protos, first = [], None
    depth = 0
    pos = 0
    for line in text.splitlines(keepends=True):
        if depth == 0:
            m = FUNC.match(line)
            if m and m.group('name') not in SKIP and m.group('ret').split()[0] not in ('typedef', 'return', 'else'):
                if first is None:
                    first = pos
                args = re.sub(r'\s*=\s*[^,)]+', '', m.group('args')).strip() or 'void'
                protos.append('%s%s(%s);' % (m.group('ret'), m.group('name'), args))
        depth += line.count('{') - line.count('}')
        pos += len(line)
    if first is None:
        first = 0
    out = text[:first] + '// prototypes generated like the Arduino IDE does\n' + '\n'.join(protos) + '\n' + text[first:]
    open(dst, 'w').write(out)


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
