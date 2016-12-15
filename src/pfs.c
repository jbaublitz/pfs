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

int ns_ramfs_mount(char *path, pfs *p) {
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

	if (mkdir(path, 0700) < 0 && errno != EEXIST) {
		printf("Failed to make namespaced ramfs mount point: %s\n",
				strerror(errno));
		return -1;
	}
	if (mount("ramfs", path, "ramfs", 0, NULL) < 0) {
		printf("Failed to mount namespaced ramfs: %s\n", strerror(errno));
		return -1;
	}
	if (mount(NULL, path, NULL, MS_PRIVATE, NULL) < 0) {
		printf("Failed to make namespaced ramfs private: %s\n", strerror(errno));
		return -1;
	}
	if (chown(path, p->user, p->group) < 0) {
		printf("Failed to change namespaced ramfs owner: %s\n", strerror(errno));
		return -1;
	}
	if (chmod(path, 0700) < 0) {
		printf("Failed to change namespaced ramfs permissions: %s\n",
				strerror(errno));
		return -1;
	}
}

int ns_ramfs_clone_exec(pfs *p) {
	char ns_path[512];
	snprintf(ns_path, sizeof(ns_path), "/proc/%d/ns/mnt", getpid());
	int nfd = open(ns_path, O_RDONLY);
	if (nfd < 0) {
		printf("Failed to open namespace pseudo file\n");
		return -1;
	}
	setns(nfd, CLONE_NEWNS);

	if (setuid(p->user) < 0) {
		printf("Failed to drop privileges\n");
		return -1;
	}
	if (seteuid(p->user) < 0) {
		printf("Failed to drop privileges\n");
		return -1;
	}

	char **argv = malloc(p->argc);
	if (!argv) {
		printf("Failed to allocate memory for execv call\n");
		return -1;
	}

	int i;
	for (i = 0; i < p->argc; i++) {
		argv[i] = p->argv[i + 1];
	}
	argv[p->argc - 1] = NULL;
	execv(p->argv[1], argv);
	return -1;
}

int ns_forker(pfs *p, int (*callback)(pfs *)) {
	pid_t pid = fork();
	if (pid < 0) {
		printf("Failed to fork: %s\n", strerror(errno));
		return 1;
	} else if (pid == 0) {
		if (callback(p) < 0)
			return 1;
	} else {
		int status;
		if (waitpid(pid, &status, 0) < 0) {
			printf("Failed to wait on child: %s\n", strerror(errno));
			return 1;
		}
		if (!WIFEXITED(status)) {
			printf("Child exited abnormally\n");
			return 1;
		}
	}

	return 0;
}

int ns_ramfs(pfs *p) {
	char path[4096];
	snprintf(path, sizeof(path), "%s/%d", PFS_MOUNT_POINT, getpid());

	if (ns_ramfs_mount(path, p) < 0)
		return -1;
	if (ns_forker(p, ns_ramfs_clone_exec) < 0)
		return -1;

	umount(path);
	umount(PFS_MOUNT_POINT);
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

	if (ns_ramfs(&p) < 0)
		return 1;

	return 0;
}
