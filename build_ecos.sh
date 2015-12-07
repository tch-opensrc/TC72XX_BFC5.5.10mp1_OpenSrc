#!/bin/sh

TOOLCHAIN_TOP=/usr/local/ecos20/gnutools/mipsisa32-elf
TOOLCHAIN_BIN=$TOOLCHAIN_TOP/bin
TOOLCHAIN_BIN_PATH=`echo $PATH | grep "$TOOLCHAIN_BIN"`

if [ -z "$TOOLCHAIN_BIN_PATH" ]; then
	export PATH=$TOOLCHAIN_BIN:$PATH
fi

[ -d rbb_cm_ecos ] || tar xvf ProdD30_BFC5.5.10_eCos_opensrc.tar.bz2
cd rbb_cm_ecos/ecos-src/bcm33xx_ipv6
sh build.bash
