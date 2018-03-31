bindir = ./bin
installdir = /opt/iecd

.PHONY: all quemngr sockwrite iecproxy ieclink scripts clean install

all: mkdirbin quemngr sockwrite iecproxy ieclink scripts

mkdirbin:
	mkdir -p ./bin

quemngr:
	$(MAKE) -C $@

sockwrite:
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
	mkdir -p $(installdir)/db_active $(installdir)/db_archive $(installdir)/log
	mkdir -p $(installdir)/var/smsdrop $(installdir)/var/run

clean:
	cd $(bindir) && rm -rf *
