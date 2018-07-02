#!/bin/sh

iconv -futf8 -tkoi8r smsget   | preconv -ekoi8r | groff -p -Tps -mru > smsget.ps
iconv -futf8 -tkoi8r quemngr  | preconv -ekoi8r | groff -p -Tps -mru > quemngr.ps
iconv -futf8 -tkoi8r iecproxy | preconv -ekoi8r | groff -p -Tps -mru > iecproxy.ps
ps2pdf smsget.ps
ps2pdf quemngr.ps
ps2pdf iecproxy.ps
rm *.ps
