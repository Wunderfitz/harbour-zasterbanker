# harbour-zasterbanker - personal finances for Sailfish OS
#
# TARGET names the binary, the main QML file, the .desktop file, the icons and
# the translations alike; renaming it means renaming all of those.

TARGET = harbour-zasterbanker

CONFIG += sailfishapp c++11
QT += sql
PKGCONFIG += sailfishsecrets

# The FinTS product registration number is baked into the binary. The header
# is kept out of Git; see src/FinTsRegistration.h.example.
!exists($$PWD/src/FinTsRegistration.h) {
    error("src/FinTsRegistration.h is missing. Copy src/FinTsRegistration.h.example and enter the registration number.")
}

# AqBanking and everything below it is built as static libraries by
# 3rdparty/build-deps.sh, once per architecture, in the build engine.
ZB_ARCH = $$system(uname -m)
contains(ZB_ARCH, armv7.*): ZB_ARCH = armv7hl
contains(ZB_ARCH, i.86): ZB_ARCH = i486
ZB_DEPS = $$PWD/3rdparty/install/$$ZB_ARCH
!exists($$ZB_DEPS/deps.pri) {
    error("Third-party libraries for $$ZB_ARCH are missing. Run 3rdparty/build-deps.sh in the build engine first (see CLAUDE.md).")
}
include($$ZB_DEPS/deps.pri)
INCLUDEPATH += $$ZB_DEPS/include
# Gwenhywfar's plugin loader; the app ships no plugins, but the code is linked in.
LIBS += -ldl

# The shim speaks AqBanking's internal aqhbci API, which is not installed;
# its headers come from the unpacked, built source tree, and include each
# other by bare name.
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

SOURCES += \
    src/main.cpp \
    src/AccountModel.cpp \
    src/AqHbciShim.c \
    src/Banking.cpp \
    src/BankingThread.cpp \
    src/EncryptedConfigStore.cpp \
    src/GwenGui.cpp \
    src/InstituteSearch.cpp \
    src/InstituteSearchWorker.cpp \
    src/KeyStore.cpp \
    src/TanImageProvider.cpp

HEADERS += \
    src/AccountModel.h \
    src/AqHbciShim.h \
    src/Banking.h \
    src/BankingThread.h \
    src/EncryptedConfigStore.h \
    src/FinTsRegistration.h \
    src/GwenGui.h \
    src/InstituteSearch.h \
    src/InstituteSearchWorker.h \
    src/KeyStore.h \
    src/TanImageProvider.h

DISTFILES += \
    qml/harbour-zasterbanker.qml \
    qml/cover/CoverPage.qml \
    qml/components/AboutLink.qml \
    qml/components/AboutParagraph.qml \
    qml/js/Format.js \
    qml/pages/AboutPage.qml \
    qml/pages/CredentialsDialog.qml \
    qml/pages/InstituteSearchPage.qml \
    qml/pages/MessageDialog.qml \
    qml/pages/OverviewPage.qml \
    qml/pages/PinDialog.qml \
    qml/pages/SetupPage.qml \
    qml/pages/TanDialog.qml \
    qml/pages/TanMethodDialog.qml \
    harbour-zasterbanker.desktop \
    rpm/harbour-zasterbanker.spec \
    rpm/harbour-zasterbanker.changes \
    translations/*.ts \
    src/FinTsRegistration.h.example \
    tools/import-banklist.py \
    tools/po2mo.py \
    tests/search-focus.sh \
    3rdparty/build-deps.sh \
    3rdparty/patches/*.patch \
    README.md \
    CLAUDE.md

# sailfishapp.prf deploys qml/ for us but knows nothing about anything else.
# QML reaches the cover artwork as ../../images, which holds both here and
# under /usr/share/harbour-zasterbanker.
images.files = images/background.png images/background-black.png images/zaster.png
images.path = /usr/share/$${TARGET}/images

# Generated from the Deutsche Kreditwirtschaft's institute list by
# tools/import-banklist.py; not in Git.
!exists($$PWD/db/fints_institutes.db) {
    error("db/fints_institutes.db is missing. Generate it with tools/import-banklist.py (see CLAUDE.md).")
}
database.files = db/fints_institutes.db
database.path = /usr/share/$${TARGET}/db

# AqBanking's FinTS message definitions. The path is compiled into AqBanking:
# its data search dir (--with-data-searchdir in 3rdparty/build-deps.sh) plus
# aqbanking/backends/aqhbci.
aqhbci.files = $$ZB_AQHBCI_XML
aqhbci.path = /usr/share/$${TARGET}/aqbanking/data/aqbanking/backends/aqhbci

# Gwenhywfar's and AqBanking's translations (PIN and TAN prompts, the bank's
# messages), compiled by 3rdparty/build-deps.sh. The path is compiled into
# both libraries: --with-locale-searchdir.
!exists($$ZB_LOCALE_DIR/de/LC_MESSAGES/aqbanking.mo) {
    error("AqBanking's translations for $$ZB_ARCH are missing. Run 3rdparty/build-deps.sh again (see CLAUDE.md).")
}
locale.files = $$ZB_LOCALE_DIR/*
locale.path = /usr/share/$${TARGET}/locale

INSTALLS += images database aqhbci locale

SAILFISHAPP_ICONS = 86x86 108x108 128x128 172x172

CONFIG += sailfishapp_i18n
TRANSLATIONS += translations/harbour-zasterbanker-de.ts
