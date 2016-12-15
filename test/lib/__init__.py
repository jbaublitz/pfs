import os

stringcontents = '''this\nis\na test'''

def filename(pid):
    return '/var/lib/ramfs-ns/{0}/test.txt'.format(pid)
