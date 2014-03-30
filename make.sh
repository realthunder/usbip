#/bin/sh
ver=`uname -r`
CONFIG_USBIP_CORE=m \
CONFIG_USBIP_VHCI_HCD=m \
CONFIG_USBIP_HOST=m \
make -C /lib/modules/$ver/build M=$PWD "$@"

