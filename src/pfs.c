#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sched.h>
#include <wait.h>
#include <fcntl.h>
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
		printf("Change executable permissions to root and enable"
				"setuid bit instead\n");
		return -1;
	}

	p->user = uid;
	p->group = gid;
	return 0;
}

int ns_ramfs_mount_exec(pfs *p) {
	snprintf(p->path, MOUNT_PATH_SIZE, "%s/%d", PFS_MOUNT_POINT, getpid());

	if (mkdir(PFS_MOUNT_POINT, 0755) < 0 && errno != EEXIST) {
		printf("Failed to make ramfs mount point: %s\n", strerror(errno));
		return -1;
	}
	if (errno == EEXIST)
		errno = 0;
	if (mount("ramfs", PFS_MOUNT_POINT, "ramfs", 0, NULL) < 0) {
		printf("Failed to mount top level ramfs: %s\n", strerror(errno));
		return -1;
	}
	if (mount(NULL, PFS_MOUNT_POINT, NULL, MS_PRIVATE, NULL) < 0) {
		printf("Failed to make top level ramfs private: %s\n", strerror(errno));
		return -1;
	}

	if (mkdir(p->path, 0700) < 0 && errno != EEXIST) {
		printf("Failed to make namespaced ramfs mount point: %s\n",
				strerror(errno));
		return -1;
	}
	if (mount("ramfs", p->path, "ramfs", 0, NULL) < 0) {
		printf("Failed to mount namespaced ramfs: %s\n", strerror(errno));
		return -1;
	}
	if (mount(NULL, p->path, NULL, MS_PRIVATE, NULL) < 0) {
		printf("Failed to make namespaced ramfs private: %s\n", strerror(errno));
		return -1;
	}
	if (chown(p->path, p->user, p->group) < 0) {
		printf("Failed to change namespaced ramfs owner: %s\n", strerror(errno));
		return -1;
	}
	if (chmod(p->path, 0700) < 0) {
		printf("Failed to change namespaced ramfs permissions: %s\n",
				strerror(errno));
		return -1;
	}

	if (setuid(p->user) < 0) {
		printf("Failed to drop privileges\n");
		return -1;
	}
	if (seteuid(p->user) < 0) {
		printf("Failed to drop privileges\n");
		return -1;
	}

	execv(p->argv[1], p->argv + 1);
	return -1;
}

int set_mnt_ns(pfs *p) {
    if (unshare(CLONE_NEWNS) < 0) {
        printf("Failed to create new mount namespace");
        return -1;
    }
    if (ns_ramfs_mount_exec(p) < 0)
        return -1;
}

int ns_forker(pfs *p, int (*child_callback)(pfs *p)) {
	pid_t pid = fork();
	if (pid < 0) {
		printf("Failed to fork: %s\n", strerror(errno));
		return 1;
	} else if (pid == 0) {
		if (child_callback(p) < 0)
			return 1;
	} else {
		int status;
		if (waitpid(pid, &status, 0) < 0) {
			printf("Failed to wait on pid namespace: %s\n", strerror(errno));
			return 1;
		}
		if (!WIFEXITED(status)) {
			printf("Some processes exited abnormally\n");
		}
	}

	return 0;
}

int set_pid_ns(pfs *p) {
    int rc = 0;
    if (unshare(CLONE_NEWPID) < 0) {
        printf("Failed to create new PID namespace");
        return -1;
    }
    if (ns_forker(p, set_mnt_ns) < 0)
        return -1;

    umount(p->path);
    umount(PFS_MOUNT_POINT);

    return rc;
}

void usage() {
	printf("USAGE: pfs EXEC [ARG...]\n");
	printf("\tEXEC\tExecutable to which to expose temporary in-memory filesystem\n");
	printf("\tARG\tArgs to pass to executable\n");
	exit(1);
}

int main(int argc, char *argv[]) {
	pfs p = {
		.argc = argc,
		.argv = argv
	};

	if (argc < 2)
		usage();

	int r = check_privs(&p);
	if (r < 0)
		return 1;

	if (set_pid_ns(&p) < 0)
		return 1;

	return 0;
}
