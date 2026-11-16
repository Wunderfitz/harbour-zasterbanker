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

# AqBanking and the libraries below it, and the data files of AqBanking's that
# go into the package. Shared with the tests (tests/engine/engine-tests.pro).
include(3rdparty/aqbanking.pri)

# For the debug log, which names the build; the spec passes both.
isEmpty(ZB_VERSION): ZB_VERSION = unknown
!isEmpty(ZB_RELEASE): ZB_VERSION = $${ZB_VERSION}-$${ZB_RELEASE}
DEFINES += ZB_VERSION=\\\"$${ZB_VERSION}\\\"

SOURCES += \
    src/main.cpp \
    src/AccountModel.cpp \
    src/AppSettings.cpp \
    src/AqHbciShim.c \
    src/Banking.cpp \
    src/BankingThread.cpp \
    src/DebugLog.cpp \
    src/EncryptedConfigStore.cpp \
    src/FlickerCode.cpp \
    src/GwenGui.cpp \
    src/InstituteSearch.cpp \
    src/InstituteSearchWorker.cpp \
    src/KeyStore.cpp \
    src/LogMasker.cpp \
    src/StatementReader.cpp \
    src/TanImageProvider.cpp \
    src/TransactionModel.cpp \
    src/HoldingModel.cpp

HEADERS += \
    src/AccountModel.h \
    src/AppSettings.h \
    src/AqHbciShim.h \
    src/Banking.h \
    src/BankingThread.h \
    src/DebugLog.h \
    src/EncryptedConfigStore.h \
    src/FinTsRegistration.h \
    src/FlickerCode.h \
    src/GwenGui.h \
    src/InstituteSearch.h \
    src/InstituteSearchWorker.h \
    src/KeyStore.h \
    src/LogMasker.h \
    src/StatementReader.h \
    src/TanImageProvider.h \
    src/TransactionModel.h \
    src/HoldingModel.h

DISTFILES += \
    qml/harbour-zasterbanker.qml \
    qml/cover/CoverPage.qml \
    qml/components/AboutLink.qml \
    qml/components/AboutParagraph.qml \
    qml/components/BankTotal.qml \
    qml/components/FlickerCode.qml \
    qml/js/Format.js \
    qml/pages/AboutPage.qml \
    qml/pages/CredentialsDialog.qml \
    qml/pages/DebugLogPage.qml \
    qml/pages/DepotPage.qml \
    qml/pages/InstituteSearchPage.qml \
    qml/pages/MessageDialog.qml \
    qml/pages/OverviewPage.qml \
    qml/pages/PinDialog.qml \
    qml/pages/SettingsPage.qml \
    qml/pages/SetupPage.qml \
    qml/pages/TanDialog.qml \
    qml/pages/TanMethodDialog.qml \
    qml/pages/TransactionsPage.qml \
    harbour-zasterbanker.desktop \
    rpm/harbour-zasterbanker.spec \
    rpm/harbour-zasterbanker.changes \
    translations/*.ts \
    src/FinTsRegistration.h.example \
    tools/import-banklist.py \
    tools/po2mo.py \
    tests/search-focus.sh \
    3rdparty/build-deps.sh \
    3rdparty/aqbanking.pri \
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

INSTALLS += images database

SAILFISHAPP_ICONS = 86x86 108x108 128x128 172x172

CONFIG += sailfishapp_i18n
TRANSLATIONS += translations/harbour-zasterbanker-de.ts
