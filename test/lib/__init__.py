import os

stringcontents = '''this\nis\na test'''
test_file = '/var/lib/ramfs-ns/{0}/test.txt'

def filename(pid):
    return test_file.format(pid)
