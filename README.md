# TC72XX_BFC5.5.10mp1_OpenSrc
BFC5.5.10mp1 open source release note for TC7210/TC7230 models

How to bulid the eCos library on Ubuntu linux OS

* Checkout source code from GitHub :
	* $git clone https://github.com/tch-opensrc/TC72XX_BFC5.5.10mp1_OpenSrc.git
	* $cd TC72XX_BFC5.5.10mp1_OpenSrc

* Decompress the eCos open source code :
	* $tar xvf ProdD30_BFC5.5.10_eCos.opensrc.tar.bz2

* Install toolchains under /usr/local :
	* $sudo tar zxvf usr_local__ecos20.tgz -C /usr/local

* Build eCos library :
	* $sh build_ecos.sh
	
* All the eCos library will be created on directory :
	* rbb_cm_ecos/ecos-src/bcm33xx_ipv6/bcm33xx_ipv6_install/lib
