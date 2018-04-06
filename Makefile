CC=clang
SRCDIR=src
BINDIR=bin
PFSBIN=pfs
PFSBINPATH=$(BINDIR)/pfs
PFSSRC=pfs.c
PFSSRCPATH=$(PFSSRC:%=SRCDIR/%)
PFSOBJ=$(PFSSRC:.c=.o)
PFSOBJPATH=$(PFSOBJ:%=$(BINDIR)/%)

all: bindir $(PFSBINPATH)

$(PFSBINPATH): $(PFSOBJPATH)
	$(CC) -o $@ $^

$(BINDIR)/%.o: $(SRCDIR)/%.c
	$(CC) -D_GNU_SOURCE -o $@ -c $<

.PHONY: bindir
bindir:
	@mkdir -p ./$(BINDIR)

.PHONY: clean
clean:
	rm $(PFSOBJPATH) $(PFSBINPATH)

.PHONY: sticky
sticky:
	chown root:root $(PFSBINPATH)
	chmod +s $(PFSBINPATH)
