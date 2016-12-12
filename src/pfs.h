#ifndef PFS_H
#define PFS_H

#define STACKSIZE (1 << 20)

typedef struct pfs_t {
	int argc;
	char **argv;
	uid_t user;
	uid_t group;
} pfs;

#endif
