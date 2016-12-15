import os

stringcontents = '''this\nis\na test'''
f = '/var/lib/ramfs-ns/{0}/test.txt'.format(os.getppid())
