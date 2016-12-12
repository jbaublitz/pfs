#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sched.h>
#include <string.h>
#include <errno.h>

#include <sys/stat.h>
#include <sys/mount.h>

#include "pfs.h"

#define PFS_MOUNT_POINT "/var/lib/ramfs-ns"

int check_privs(pfs *p) {
	int perms = 0;
	uid_t uid = getuid();
	uid_t gid = getgid();
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
		printf("Change executable permissions to root and enable setuid bit instead\n");
		return -1;
	}

	p->user = uid;
	p->group = gid;
	return 0;
}

int ns_ramfs_mount(pfs *p) {
	if (mkdir(PFS_MOUNT_POINT, 0700) < 0 && errno != EEXIST) {
		printf("Failed to make ramfs mount point: %s\n", strerror(errno));
		return -1;
	}
	if (errno == EEXIST)
		errno = 0;

	char data[32];
	snprintf(data, sizeof(data), "uid=%d,gid=%d", p->user, p->group);
	if (mount("ramfs", PFS_MOUNT_POINT, "ramfs", 0, data) < 0) {
		return -1;
	}
	if (mount(NULL, PFS_MOUNT_POINT, NULL, MS_PRIVATE, NULL) < 0) {
		return -1;
	}
}

int ns_ramfs_fork_exec(pfs *p) {
	execv(p->argv[1], p->argv + 1);
}

int ns_ramfs_wrapper(void *arg) {
	pfs *p = (pfs *)arg;
	unshare(CLONE_NEWNS);

	ns_ramfs_mount(p);
	//ns_ramfs_fork_exec(p);
	umount(PFS_MOUNT_POINT);
}

int main(int argc, char *argv[]) {
	if (argc < 2)
		return 1;

	pfs p;

	int r = check_privs(&p);
	if (r < 0)
		return 1;

	char *cstack = malloc(STACKSIZE);

	if (clone(ns_ramfs_wrapper, cstack + STACKSIZE, CLONE_NEWNS, (void *)&p) < 0) {
		printf("Failed to create namespace for filesystem: %s\n", strerror(errno));
	}
}
