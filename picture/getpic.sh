#!/bin/sh

for i in *.roff; do 
	fname=$(basename -s .roff $i)
	iconv -futf8 -tkoi8r "$i" | preconv -ekoi8r | groff -p -Tps -mru > "${fname}.ps"
	ps2pdf ${fname}.ps
done
