#!/usr/bin/python

import os
import sys
import subprocess
import unittest

from lib import filename, stringcontents
from lib.fileutils import read_file

class AccessUnitTest(unittest.TestCase):
    def test_pfs_namespaces_sudo(self):
        pwd = os.environ.get('PWD') or '.'
        user = os.environ.get('USER')
        if user is None:
            print('User could not be found')
            sys.exit(1)
        p = subprocess.Popen(['sudo', '-E', './bin/pfs', '-c',
                os.path.join(pwd, 'test', 'exec.py'), '-u', user],
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        stdout, stderr = p.communicate()
        print(stdout.decode('utf-8'), end="")
        if p.returncode != 0:
            sys.exit(1)

        f = filename(1)
        print('Checking file cannot be read outside of namespace...')
        try:
            filecontents = open(f).read()
            if filecontents == stringcontents:
                sys.exit(1)
        except IOError as e:
            pass

if __name__ == '__main__':
    unittest.main()
