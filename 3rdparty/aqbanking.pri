# What Zaster Banker needs of AqBanking and the libraries below it, for the app
# (harbour-zasterbanker.pro) and the tests (tests/engine/engine-tests.pro)
# alike: compiler and linker flags, the shim's internal headers, the statement
# parser, and the data files of AqBanking's that go into the package. The tests
# install the latter into a folder of their own and read them from there, so
# they see exactly what the package holds.
#
# Paths are fixed below /usr/share/harbour-zasterbanker, as they are compiled
# into the libraries (3rdparty/build-deps.sh), whatever the including project's
# TARGET is.

# AqBanking and everything below it is built as static libraries by
# 3rdparty/build-deps.sh, once per architecture, in the build engine.
ZB_ARCH = $$system(uname -m)
contains(ZB_ARCH, armv7.*): ZB_ARCH = armv7hl
contains(ZB_ARCH, i.86): ZB_ARCH = i486
ZB_DEPS = $$PWD/install/$$ZB_ARCH
!exists($$ZB_DEPS/deps.pri) {
    error("Third-party libraries for $$ZB_ARCH are missing. Run 3rdparty/build-deps.sh in the build engine first (see CLAUDE.md).")
}
include($$ZB_DEPS/deps.pri)
INCLUDEPATH += $$ZB_DEPS/include
# Gwenhywfar's plugin loader; the app ships no plugins, but the code is linked in.
LIBS += -ldl
# AqBanking's parser for bank statements (MT940/MT942). AqBanking builds it as a
# plugin only; BankingThread registers it with Gwenhywfar instead of loading it.
ZB_SWIFT_PARSER = $$ZB_DEPS/lib/aqbanking/plugins/44/dbio/swift.a
!exists($$ZB_SWIFT_PARSER) {
    error("AqBanking's statement parser for $$ZB_ARCH is missing: $$ZB_SWIFT_PARSER")
}
# It needs AqBanking and Gwenhywfar, so it goes in front of them; qmake would drop
# a second -lgwenhywfar behind it, and with that the first one's place in the order.
LIBS = $$ZB_SWIFT_PARSER $$LIBS

# The shim speaks AqBanking's internal aqhbci API, which is not installed;
# build-deps.sh copies its headers from AqBanking's source tree next to the
# libraries (aqbanking-internal), and they include each other by bare name.
ZB_AQHBCI_SRC = $$ZB_AQBANKING_SRC/src/libs/plugins/backends/aqhbci
INCLUDEPATH += \
    $$ZB_AQBANKING_SRC/src/libs/plugins/backends \
    $$ZB_AQBANKING_SRC/src/libs \
    $$ZB_AQHBCI_SRC \
    $$ZB_AQHBCI_SRC/msglayer \
    $$ZB_AQHBCI_SRC/banking \
    $$ZB_AQHBCI_SRC/joblayer \
    $$ZB_AQHBCI_SRC/applayer \
    $$ZB_AQHBCI_SRC/tan

# AqBanking's FinTS message definitions. The path is compiled into AqBanking:
# its data search dir (--with-data-searchdir in 3rdparty/build-deps.sh) plus
# aqbanking/backends/aqhbci.
aqhbci.files = $$ZB_AQHBCI_XML
aqhbci.path = /usr/share/harbour-zasterbanker/aqbanking/data/aqbanking/backends/aqhbci

# Gwenhywfar's and AqBanking's translations (PIN and TAN prompts, the bank's
# messages), compiled by 3rdparty/build-deps.sh. The path is compiled into
# both libraries: --with-locale-searchdir.
!exists($$ZB_LOCALE_DIR/de/LC_MESSAGES/aqbanking.mo) {
    error("AqBanking's translations for $$ZB_ARCH are missing. Run 3rdparty/build-deps.sh again (see CLAUDE.md).")
}
locale.files = $$ZB_LOCALE_DIR/*
locale.path = /usr/share/harbour-zasterbanker/locale

# How to read the bank statements. The path is compiled into AqBanking, as above.
swiftprofiles.files = $$ZB_DEPS/share/aqbanking/imexporters/swift/profiles/*
swiftprofiles.path = /usr/share/harbour-zasterbanker/aqbanking/data/aqbanking/imexporters/swift/profiles

# The same for CAMT statements: the importer's profiles and the schemas. AqBanking
# derives the formats it can offer the bank from the camt_* profiles' names; without
# them it silently drops the CAMT job.
camtprofiles.files = $$ZB_DEPS/share/aqbanking/imexporters/xml/profiles/default.conf \
    $$ZB_DEPS/share/aqbanking/imexporters/xml/profiles/camt_*.conf
camtprofiles.path = /usr/share/harbour-zasterbanker/aqbanking/data/aqbanking/imexporters/xml/profiles
camtschemas.files = $$ZB_DEPS/share/aqbanking/imexporters/xml/data/camt_*.xml
camtschemas.path = /usr/share/harbour-zasterbanker/aqbanking/data/aqbanking/imexporters/xml/data

INSTALLS += aqhbci swiftprofiles camtprofiles camtschemas locale
