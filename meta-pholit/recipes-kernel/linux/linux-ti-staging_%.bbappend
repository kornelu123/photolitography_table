FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI += "file://usb-serial.cfg \
            file://0001-arm64-dts-k3-j721e-common-proc-board-Add-INA2xx-supp.patch \
            file://0001-arm64-dts-ti-k3-j721e-sk-Enable-wkup-i2c0.patch \
            file://0001-Adjust-device-tree-files-for-pgraphy-purposes.patch \
            file://0002-Change-default-kernel-config-file.patch \
            file://0001-Add-i2c-and-gpio-pins-support-in-dts-file.patch \
            "
