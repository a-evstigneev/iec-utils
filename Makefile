bindir = ./bin
installdir = /opt/iecd

.PHONY: all quemngr asdusend iecproxy ieclink scripts clean install

all: mkdirbin quemngr asdusend iecproxy ieclink scripts

mkdirbin:
	mkdir -p ./bin

quemngr:
	$(MAKE) -C $@

asdusend:
	$(MAKE) -C $@

iecproxy: 
	$(MAKE) -C $@

ieclink:
	$(MAKE) -C $@

scripts:
	cp -r ./$@/* $(bindir)

install:
	rm -rf $(installdir)/*
	cp -r $(bindir)/* $(installdir) 
	mkdir -p $(installdir)/iecdb $(installdir)/log
	mkdir -p $(installdir)/var/smsdrop $(installdir)/var/run

clean:
	cd $(bindir); rm -rf *
