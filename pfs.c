#include <stdio.h>
#include <unistd.h>

#include <fuse.h>

struct pfs_t {
	uid_t user;
	pid_t pgroup;
} pfs;

int main(int argc, char *argv[]) {
	int perms = 0;
	uid_t uid = getuid();
	uid_t euid = geteuid();
	if (uid == 0) {
		printf("Do not run as root!\n");
		perms = 1;
	}
	if (euid != 0) {
		printf("Need root permissions to mount underlying ramfs!\n");
		perms = 1;
	}
	if (perms) {
		printf("Change executable permissions to root and enable setuid bit instead.\n");
		return 1;
	}

	pfs.user = uid;
	pfs.pgroup = getpgrp();

	fuse_main(argc, argv, (void *)&pfs);
}
