#!/tools/bin/bash

#****************************************************************************
#
# Copyright (c) 2009-2010 Broadcom Corporation
#
# This file being released as part of eCos, the Embedded Configurable Operating System.
#
# eCos is free software; you can redistribute it and/or modify it under
# the terms of the GNU General Public License version 2, available at 
# http://www.broadcom.com/licenses/GPLv2.php (the "GPL").
#
# As a special exception, if you compile this file and link it
# with other works to produce a work based on this file, this file does not
# by itself cause the resulting work to be covered by the GNU General Public
# License. However the source code for this file must still be made available
# in accordance with section (3) of the GNU General Public License.
#
#****************************************************************************

# sample eCos library build process

# this script builds the eCos library clean and copies modified files to the 
# application source tree

chmod +w bcm33xx_smp.ecc
cd bcm33xx_smp_build/
../../ecos-2.0/tools/bin/ecosconfig --config=../bcm33xx_smp.ecc --prefix=$(pwd)/../bcm33xx_smp_install --srcdir=$(pwd)/../../ecos-2.0/packages --no-resolve tree
rm -fv ../bcm33xx_smp_install/lib/*.{a,o}
make -s clean
make -s -j 8
diff -qr -x target.ld ../bcm33xx_smp_install/ ../../../../rbb_cm_src/Bfc/LibSupport/eCos/bcm33xx_smp_install/ | grep differ | cut -d " " -f 2,4 | xargs -n2 cp -pfv
