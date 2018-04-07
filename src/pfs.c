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

#define MOUNT_PATH_SIZE 512

typedef struct pfs_t {
    int argc_pfs;
    char **argv_pfs;
    uid_t user;
    uid_t group;
    char path[MOUNT_PATH_SIZE];
} pfs;

int check_privs(pfs *p) {
    int perms = 0;
    uid_t uid = getuid();
    uid_t euid = geteuid();
    if (uid == 0 && !p->user) {
        printf("Cannot run as root without -u parameter\n");
        return -1;
    }
    if (euid != 0) {
        printf("Need root permissions to mount underlying ramfs!\n");
        return -1;
    }

    return 0;
}

int set_mnt_ns_exec(pfs *p) {
    if (unshare(CLONE_NEWNS) < 0) {
        printf("Failed to create new mount namespace\n");
        return -1;
    }

    if (mkdir(p->path, 0700) < 0) {
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

    execv(p->argv_pfs[0], p->argv_pfs);
    printf("Failed to exec %s\n", p->argv_pfs[0]);
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

    if (rmdir(p->path)) {
        printf("Failed to clean up on exit: %s\n", strerror(errno));
        printf("You will need to manually remove %s\n", p->path);
    }

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

    int opt, argcount = 0;
    p->argv_pfs = calloc(argc + 1, sizeof(char *));
    if (!p->argv_pfs) {
        printf("Failed to allocate buffer for command arguments");
        return -1;
    }
    int i;
    for (i = 1; i < argc; i++) {
        if (!strcmp("--", argv[i])) {
            break;
        }
        printf("%s\n", argv[i]);
        p->argv_pfs[i - 1] = argv[i];
    }
    while ((opt = getopt(argc - i, argv + i, "u:g:")) != -1) {
        switch (opt) {
            char *end = NULL;
            uid_t uid;
            uid_t gid;
            case 'u':
                uid = strtol(optarg, &end, 10);
                if (!uid && *end != '\0') {
                    usage("Failed to parse uid");
                    return -1;
                }
                p->user = uid;
                end = NULL;
                break;
            case 'g':
                gid = strtol(optarg, &end, 10);
                if (!gid && *end != '\0') {
                    usage("Failed to parse gid");
                    return -1;
                }
                p->group = gid;
                end = NULL;
                break;
            default:
                usage("Unrecognized option");
        }
    }

    if (i == 0) {
        usage("Must supply command");
        return -1;
    }

    p->argv_pfs[i] = (char *)NULL;
    if (p->group == -1)
        p->group = p->user;

    return 0;
}

int main(int argc, char *argv[]) {
    pfs p = {
        .user = 0,
        .group = -1,
    };
    snprintf(p.path, MOUNT_PATH_SIZE, "./.%s-%d", getlogin(), getpid());

    if (parse_args(argc, argv, &p) < 0) {
        free(p.argv_pfs);
        return 1;
    }

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
