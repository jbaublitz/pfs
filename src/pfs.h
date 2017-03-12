#ifndef PFS_H
#define PFS_H

#define MOUNT_PATH_SIZE 512

typedef struct pfs_t {
    int argc_pfs;
    char *command;
    char **argv_pfs;
    uid_t user;
    uid_t group;
    char path[MOUNT_PATH_SIZE];
} pfs;

#endif
