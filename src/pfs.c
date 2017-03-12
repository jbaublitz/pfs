#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sched.h>
#include <wait.h>
#include <pwd.h>
#include <fcntl.h>
#include <string.h>
#include <getopt.h>
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
    if (uid == 0 && !p->user) {
        printf("Cannot run as root without -u parameter\n");
        return -1;
    }
    if (euid != 0) {
        printf("Need root permissions to mount underlying ramfs!\n");
        return -1;
    }

    p->user = uid;
    p->group = gid;
    return 0;
}

int set_mnt_ns_exec(pfs *p) {
    if (unshare(CLONE_NEWNS) < 0) {
        printf("Failed to create new mount namespace");
        return -1;
    }

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

    execv(p->command, p->argv_pfs);
    return -1;
}

int ns_forker(pfs *p, int (*child_callback)(pfs *p)) {
    pid_t pid = fork();
    if (pid < 0) {
        printf("Failed to fork: %s\n", strerror(errno));
        return -1;
    } else if (pid == 0) {
        if (child_callback(p) < 0)
            return -1;
    } else {
        int status;
        if (waitpid(pid, &status, 0) < 0) {
            printf("Failed to wait on pid namespace: %s\n", strerror(errno));
            return -1;
        }
        if (!WIFEXITED(status)) {
            printf("Some processes exited abnormally\n");
            return 1;
        }
    }

    return 0;
}

int set_pid_ns_fork(pfs *p) {
    if (unshare(CLONE_NEWPID) < 0) {
        printf("Failed to create new PID namespace");
        return -1;
    }

    int rc = ns_forker(p, set_mnt_ns_exec);
    if (rc < 0)
        return -1;

    umount(p->path);
    umount(PFS_MOUNT_POINT);

    return rc;
}

void usage(char *msg) {
    if (msg) {
        printf("%s\n\n", msg);
    }
    printf("USAGE: pfs [-u USER] [-g GROUP] -c COMMAND [-a ARG1 [-a ARG2...]]\n");
    printf("\tUSER\tRun executable with these user permissions\n");
    printf("\tGROUP\tRun executable with these group permissions\n");
    printf("\tCOMMAND\tExecutable to which to expose temporary in-memory filesystem\n");
    printf("\tARG\tArgs to pass to executable\n");
}

int parse_args(int argc, char **argv, pfs *p) {
    if (argc < 2) {
        usage(NULL);
        return 1;
    }

    int opt, argcount = 1;
    p->argv_pfs = calloc(argc + 1, sizeof(char *));
    if (!p->argv_pfs) {
        printf("Failed to allocate buffer for command arguments");
        return -1;
    }
    while ((opt = getopt(argc, argv, "u:g:c:a:")) != -1) {
        switch (opt) {
            case 'u':
                if (!p->user) {
                    struct passwd *uinfo = getpwnam(optarg);
                    if (!uinfo) {
                        printf("Username does not exist\n");
                        goto free_argv_pfs;
                    }
                    p->user = uinfo->pw_uid;
                } else {
                    usage("Only one username allowed");
                    goto free_argv_pfs;
                }
                break;
            case 'g':
                if (!p->group) {
                    struct passwd *uinfo = getpwnam(optarg);
                    if (!uinfo) {
                        printf("Group name does not exist\n");
                        goto free_argv_pfs;
                    }
                    p->group = uinfo->pw_gid;
                } else {
                    usage("Only one group name allowed");
                    goto free_argv_pfs;
                }
                break;
            case 'c':
                if (!p->command) {
                    p->command = realpath(optarg, NULL);
                    if (!p->command) {
                        usage("Command specified does not exist");
                        goto free_argv_pfs;
                    }
                } else {
                    usage("Only one command allowed");
                    goto free_argv_pfs;
                }
                break;
            case 'a':
                p->argv_pfs[argcount++] = optarg;
                break;
            default:
                usage("Unrecognized argument");
                goto free_argv_pfs;
        }
    }

    if (!p->command) {
        usage("Must supply command");
        goto free_argv_pfs;
    }

    p->argv_pfs[0] = basename(p->command);
    p->argv_pfs[argcount] = (char *)NULL;
    if (!p->group)
        p->group = p->user;

    return 0;

free_argv_pfs:
    free(p->argv_pfs);
    return -1;
}

int main(int argc, char *argv[]) {
    pfs p = {
        .user = 0,
        .group = 0,
        .command = NULL
    };

    if (parse_args(argc, argv, &p) < 0)
        return 1;

    if (check_privs(&p) < 0) {
        free(p.argv_pfs);
        return 1;
    }

    if (set_pid_ns_fork(&p) != 0) {
        free(p.argv_pfs);
        return 1;
    }

    free(p.argv_pfs);

    return 0;
}
