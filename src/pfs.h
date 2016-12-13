#ifndef PFS_H
#define PFS_H

typedef struct pfs_t {
	int argc;
	char **argv;
	uid_t user;
	uid_t group;
} pfs;

#endif
