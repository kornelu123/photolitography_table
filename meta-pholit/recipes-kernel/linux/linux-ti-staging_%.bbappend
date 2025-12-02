FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI += " \
    file://usb-serial.cfg \
    file://0001-Adjust-device-tree-files-for-pgraphy-purposes.patch \
    file://0002-Change-default-kernel-config-file.patch \
"
