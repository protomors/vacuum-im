TARGET = voicecalls
QT    += multimedia
CONFIG += link_pkgconfig
PKGCONFIG += gstreamer-1.0 gstreamer-sdp-1.0 gstreamer-webrtc-1.0
include(voicecalls.pri)
include(../plugins.inc)
