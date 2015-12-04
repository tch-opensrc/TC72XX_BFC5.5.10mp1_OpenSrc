eCos - the Embedded Configurable Operating System - release 2.0 README
======================================================================

May 2003


Welcome to the eCos 2.0 public net release.

This README contains a list of known problems with the eCos 2.0 release.
Please check for further issues by searching the Bugzilla database for
product "eCos" version "2.0":

  http://bugzilla.redhat.com/bugzilla/query.cgi?product=eCos

If you discover new bugs with this release please report them using
Bugzilla:

  http://bugzilla.redhat.com/bugzilla/enter_bug.cgi?product=eCos


------------------------------------------------------------------------------
eCos 2.0 Errata
---------------

* When building RedBoot for ROM startup, the following warning may be produced
by BFD:

  allocated section `.bss' is not in segment

This warning is due to mishandling of overlapping segments and is harmless. A
patch has been submitted to the binutils mailing list.

* When building eCos for the 'linux' target with the 'net' template, the
following warning is produced:

  /usr/bin/ld: warning: no memory region specified for section
  `.rel.ecos.table._Net_inits.data.0x88000001domain_routecyg_net_add_domain'

This warning is harmless and may be ignored.

* eCos fails to build for target 'flexanet' since the platform support has not
been maintained with respect to the rest of the repository.

* RedBoot fails to build for target 'dreamcast' since the package
CYGPKG_FS_ISO is not present in the eCos public repository.

* The eCos tests 'pselect' and 'cpuload' may fail erroneously on some eCos
targets (false negative).

* We are aware of the following issues with specific versions of the cygwin1
DLL:

  - version 1.3.19 cannot be used to configure eCos toolchains due to a
       problem with vasprintf.
  - version 1.3.20 causes various toolchain failures when building eCos on
       Win95/98/ME only. We suspect this is an mmap issue.

* The PSIM PowerPC simulator treats all data cache instructions as noops.
The behaviour is benign with the exception of the dcbz instruction. It causes
the eCos kcache2 test to fail on target 'psim'.

* Register handling in the GDB stub code for the MIPS32 variant has been
changed to supply 32 bit registers rather than 64 bit. This change allows
use the public gcc 3.2.x compiler but will cause problems with the
mipsisa32-elf toolchain from ftp://ftp.mips.com/pub/redhat/linux/

* There are a number of minor issues with the eCos Configuration Tool:

  84946 Configtool build progress bar inoperative
  84948 Cannot add new command prefix in platform editor dialog box
  85588 Starting help spawns xterm which doesn't disappear
  85597 Configtool internal doc viewer issues
  89778 Configtool platforms list is not sorted
  90180 NT config tool configuration pane divider dragging unreliable

* The eCos documentation does not describe how to invoke the Insight graphical
debugger. Insight is invoked by specifying the "-w" switch on the GDB command
line.

* The Insight graphical debugger exhibits a lack of robustness on Win95/98/ME
hosts. We recommend using command-line GDB on these platforms.
