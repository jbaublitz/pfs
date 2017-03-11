#!/usr/bin/python

from __future__ import print_function

import os

from lib import filename, stringcontents
from lib.fileutils import write_file, read_file

def main():
    write_file(filename(os.getpid()))
    read_file(filename(os.getpid()))

if __name__ == '__main__':
    main()
