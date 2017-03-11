#!/usr/bin/python

import os
import sys
import subprocess

from lib import filename, stringcontents
from lib.fileutils import read_file

def main():
    p = subprocess.Popen(['./bin/pfs', os.path.join(os.environ['PWD'], 'test',
            'exec.py')], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    stdout, stderr = p.communicate()
    print(stdout.decode('utf-8'))
    if p.returncode != 0:
        print('FAILURE')
        sys.exit(1)

    f = filename(1)
    print('Checking file cannot be read outside of namespace...')
    try:
        filecontents = open(f).read()
        if filecontents == stringcontents:
            print('FAILURE')
    except IOError as e:
        print('SUCCESS')

if __name__ == '__main__':
    main()
