.PHONY: all iecproxy asdusend asduconv quemngr clean

bindir = bin

all: iecproxy asdusend asduconv quemngr

iecproxy: 
	$(MAKE) -C $@

asdusend:
	$(MAKE) -C $@

quemngr:
	$(MAKE) -C $@

asduconv:
	cp ./asduconv/$@.sh $(bindir)/$@.sh

clean:
	cd $(bindir); rm -rf *
