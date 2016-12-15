#!/usr/bin/python

import os
import sys
import subprocess

from lib import f, stringcontents
from lib.fileutils import read_file

def main():
    p = subprocess.Popen(['./bin/pfs', os.path.join(os.environ['PWD'], 'test',
            'exec.py')], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    stdout, stderr = p.communicate()
    print(stdout)

    filecontents = read_file(f)
    print('Checking file contents... ')
    if filecontents != stringcontents:
        print('FAILURE')
        sys.exit(1)

    print('SUCCESS')

if __name__ == '__main__':
    main()
