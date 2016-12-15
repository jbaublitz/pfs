import sys

from . import stringcontents

def write_file(f):
    print('Creating file {0} in ramfs-ns mount...'.format(f))
    try:
        f = open(f, 'w')
        f.write(stringcontents)
        f.close()
    except IOError as e:
        print("FAILURE")
        sys.exit(1)

    print("SUCCESS")

def read_file(f):
    print('Reading file {0} in ramfs-ns mount...'.format(f))
    s = None
    try:
        f = open(f, 'r')
        s = f.read()
        f.close()
    except IOError as e:
        print("FAILURE")
        sys.exit(1)

    print("SUCCESS")
