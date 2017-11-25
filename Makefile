bindir = ./bin
installdir = /opt/iecd
workdir = $(installdir)/work


.PHONY: all iecproxy asdusend quemngr ieclink scripts clean install

all: iecproxy asdusend quemngr ieclink scripts

iecproxy: 
	$(MAKE) -C $@

ieclink:
	$(MAKE) -C $@

asdusend:
	$(MAKE) -C $@

scripts:
	cp -r ./$@/* $(bindir)

quemngr:
	$(MAKE) -C $@

install:
	rm -rf /opt/iecd_with_proxy/*
	cp -r $(bindir)/* $(installdir) 
	mkdir -p $(workdir)/drop
	mkdir -p $(workdir)/fail
	mkdir -p $(workdir)/in
	mkdir -p $(workdir)/act
	mkdir -p $(workdir)/df/0h0m30s
	mkdir -p $(workdir)/df/0h0m45s
	mkdir -p $(workdir)/df/0h0m60s
	mkdir -p $(installdir)/iecdb

clean:
	cd $(bindir); rm -rf *
