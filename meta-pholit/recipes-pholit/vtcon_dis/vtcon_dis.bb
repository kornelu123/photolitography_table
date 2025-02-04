SUMMARY = "Systemd service for disabling the vtconsoles"
DESCRIPTION = "Systemd service for disabling the vtconsoles"
LICENSE = "CLOSED"

SRC_URI = " \
    file://vtcon_dis.service \
"

DEPENDS += "virtual/kernel systemd"
PROVIDES = "vtcon_dis"
RPROVIDES:${PN} = "vtcon_dis"

SYSTEMD_SERVICE:${PN} = "vtcon_dis.service"

inherit systemd allarch

do_install() {
    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${WORKDIR}/vtcon_dis.service ${D}${systemd_system_unitdir}
}

FILES:${PN} = "${systemd_system_unitdir}/vtcon_dis.service"
