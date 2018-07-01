#!/bin/sh

iconv -futf8 -tkoi8r pic6 | preconv -ekoi8r | groff -p -Tps -mru > pic6.ps
