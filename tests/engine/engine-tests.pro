# The tests that need AqBanking, Gwenhywfar and Qt 5.6 as the app has them,
# in one program. Built and run in the build engine by tests/run-engine-tests.sh;
# see CLAUDE.md, section "Tests".

TEMPLATE = app
TARGET = tst_engine
CONFIG += console c++11
CONFIG -= app_bundle
QT = core gui sql

ZB_ROOT = $$PWD/../..
# The same libraries and flags as the app, and AqBanking's data files, which
# "make install INSTALL_ROOT=..." puts where the tests read them.
include($$ZB_ROOT/3rdparty/aqbanking.pri)
INCLUDEPATH += $$ZB_ROOT/src
# As the package build links the app: GnuTLS's static flags name libatomic,
# which nothing uses on these targets and the build target has no runtime
# library of.
QMAKE_LFLAGS += -Wl,--as-needed

SOURCES += \
    main.cpp \
    FakeBankingThread.cpp \
    tst_configstore.cpp \
    tst_gwengui.cpp \
    tst_institutesearch.cpp \
    tst_models.cpp \
    tst_statements.cpp \
    tst_tanmethods.cpp \
    $$ZB_ROOT/src/AccountModel.cpp \
    $$ZB_ROOT/src/AqHbciShim.c \
    $$ZB_ROOT/src/EncryptedConfigStore.cpp \
    $$ZB_ROOT/src/GwenGui.cpp \
    $$ZB_ROOT/src/HoldingModel.cpp \
    $$ZB_ROOT/src/InstituteSearchWorker.cpp \
    $$ZB_ROOT/src/StatementReader.cpp \
    $$ZB_ROOT/src/TransactionModel.cpp

HEADERS += \
    Check.h \
    FakeBankingThread.h \
    $$ZB_ROOT/src/AccountModel.h \
    $$ZB_ROOT/src/AqHbciShim.h \
    $$ZB_ROOT/src/BankingThread.h \
    $$ZB_ROOT/src/EncryptedConfigStore.h \
    $$ZB_ROOT/src/GwenGui.h \
    $$ZB_ROOT/src/HoldingModel.h \
    $$ZB_ROOT/src/InstituteSearchWorker.h \
    $$ZB_ROOT/src/StatementReader.h \
    $$ZB_ROOT/src/TransactionModel.h
