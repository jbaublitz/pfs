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
	gcc -o $@ $^

$(BINDIR)/%.o: $(SRCDIR)/%.c $(SRCDIR)/%.h
	gcc -D_GNU_SOURCE -o $@ -c $<

.PHONY:
bindir:
	@mkdir -p ./$(BINDIR)

.PHONY:
clean:
	rm $(PFSOBJPATH) $(PFSBINPATH)

.PHONY:
sticky:
	chown root:root $(PFSBINPATH)
	chmod +s $(PFSBINPATH)
