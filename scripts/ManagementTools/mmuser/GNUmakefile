
prefix  ?= /usr/local
bindir  := $(prefix)/bin
destdir := $(prefix)/mmutils/mmuser

files   := $(wildcard *.yml *.rb *.cfg) add-to-wheel.sh
script  := mmuser.sh
command := mmuser

install:
	install -o root -g root  -m 755 -d $(bindir)
	install -o root -g wheel -m 775 -d $(destdir)
	install -o root -g root  -m 755 $(script) $(destdir)
	install -o root -g root  -m 644 $(files)  $(destdir)
	ln -sf $(destdir)/$(script) $(bindir)/$(command)

clean:
	rm -f inventory userdata.yml

distclean:
	rm -fr $(destdir) $(bindir)/$(command)
