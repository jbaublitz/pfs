import os
import sys

from . import stringcontents, test_file

def write_file(f):
    print('Creating file in ramfs-ns mount...')
    try:
        f = open(f, 'w')
        f.write(stringcontents)
        f.close()
    except IOError as e:
        sys.exit(1)

def read_file(f):
    print('Reading file in ramfs-ns mount...')
    s = None
    try:
        f = open(f, 'r')
        s = f.read()
        f.close()
    except IOError as e:
        sys.exit(1)
