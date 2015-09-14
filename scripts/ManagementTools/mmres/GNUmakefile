# GNUmakefile

prefix := /usr/local
dest   := $(prefix)/mmres
bindir := $(prefix)/bin
tmpdir := /tmp

wrapper := run-script
hook    := cleanup-hosts.sh

scripts  := $(wildcard *.py)
lockfile := $(wildcard lockfile/*.py)
dbdir    := ./db
dbfile   := $(dbdir)/activeLeaseDB

install: $(wrapper) $(dbfile)
	install -o root -m 4755 run-script $(bindir)
	install -o root -m  755 $(hook)    $(bindir)/$(hook:%.sh=%)
	install -d -o root -m 755 $(dest)/lockfile
	install -d -o root -m 777 $(dest)/db
	install -o root -m 666 $(dbfile) $(dest)/db
	install -o root $(scripts)  $(dest)
	install -o root $(lockfile) $(dest)/lockfile
	install -o root -m 755 mmres.py $(dest)
	ln -sf $(dest)/mmres.py $(bindir)/mmres

$(wrapper): $(wrapper:%=%.c)
	$(CC) -DSCRIPT=\"$(bindir)/$(hook:%.sh=%)\" -o $@ $^

test: $(tmpdir)/$(hook:%.sh=%)
	TESTING=yes python mmres_test.py

$(tmpdir)/$(hook:%.sh=%): $(hook)
	sed 's/ *ssh/echo ssh/g' $^ > $@
	chmod 755 $@

$(dbfile):
	[ -d $(dbdir) ] || mkdir $(dbdir)
	TESTING=yes python mmres.py ls > /dev/null

clean:
	rm -f run-script *.pyc lockfile/*.pyc
	rm -fr $(dbdir)
	rm -f $(tmpdir)/$(hook:%.sh=%)

distclean:
	rm -f $(bindir)/mmres $(bindir)/run-script $(bindir)/$(hook:%.sh=%)
	rm -fr $(dest)
