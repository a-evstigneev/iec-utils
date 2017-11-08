bindir = ./bin
installdir = /opt/iecd_with_proxy

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
	cp -r $(bindir)/* $(installdir) 

clean:
	cd $(bindir); rm -rf *
