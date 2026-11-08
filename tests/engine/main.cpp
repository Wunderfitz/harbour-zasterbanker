/*
    Copyright (C) 2026 Sebastian J. Wolf and other contributors

    This file is part of Zaster Banker.

    Zaster Banker is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Zaster Banker is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Zaster Banker. If not, see <http://www.gnu.org/licenses/>.
*/

/*
 * The tests that need AqBanking, Gwenhywfar and Qt as the app has them. Built
 * and run in the build engine by tests/run-engine-tests.sh, which passes where
 * it installed AqBanking's data files and the made-up bank list.
 */

#include "Check.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QString>
#include <QTemporaryDir>

#include <gcrypt.h>
#include <gwenhywfar/dbio.h>
#include <gwenhywfar/gwenhywfar.h>
#include <gwenhywfar/logger.h>
#include <gwenhywfar/plugin.h>

#include <aqbanking/banking.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <pwd.h>

extern "C" GWEN_PLUGIN *dbio_swift_factory(GWEN_PLUGIN_MANAGER *pm, const char *modName, const char *fileName);

void runConfigStoreTests();
void runGwenGuiTests();
void runStatementTests(const QString &installedData);
void runModelTests();
void runInstituteSearchTests(const QString &database);

int Check::failures = 0;

namespace {
QByteArray homeFolder;
}

/*
 * AB_Banking_new() looks for an old settings folder in the home directory to
 * copy over, and aborts if it cannot find the home directory - which the build
 * engine's target cannot, as it has no passwd entry for the build user. Taking
 * the place of libc's, this gives AqBanking an empty home folder of the tests'
 * own instead, so that nothing of the developer's is read, and the tests can
 * check that nothing is written there.
 */
extern "C" struct passwd *getpwuid(uid_t uid)
{
    static struct passwd entry;
    std::memset(&entry, 0, sizeof(entry));
    entry.pw_name = const_cast<char *>("tests");
    entry.pw_uid = uid;
    entry.pw_dir = homeFolder.data();
    entry.pw_shell = const_cast<char *>("/bin/false");
    return &entry;
}

int main(int argc, char *argv[])
{
    std::setvbuf(stdout, nullptr, _IOLBF, 0);
    QCoreApplication app(argc, argv);
    if (argc != 3) {
        std::fprintf(stderr, "Usage: %s <installed /usr/share/harbour-zasterbanker> <institute database>\n", argv[0]);
        return 2;
    }
    const QString installedData = QString::fromLocal8Bit(argv[1]);
    const QString database = QString::fromLocal8Bit(argv[2]);
    QTemporaryDir home;
    homeFolder = QFile::encodeName(home.path());
    qputenv("HOME", homeFolder);

    // As main.cpp and BankingThread::run() start them.
    gcry_check_version(nullptr);
    gcry_control(GCRYCTL_INITIALIZATION_FINISHED, 0);
    GWEN_Init();
    GWEN_Logger_SetLevel(GWEN_LOGDOMAIN, GWEN_LoggerLevel_Error);
    GWEN_Logger_SetLevel(AQBANKING_LOGDOMAIN, GWEN_LoggerLevel_Error);
    GWEN_Logger_SetLevel("aqhbci", GWEN_LoggerLevel_Error);
    GWEN_PLUGIN_MANAGER *dbioPlugins = GWEN_PluginManager_FindPluginManager(GWEN_DBIO_PLUGIN_NAME);
    GWEN_PluginManager_AddPlugin(dbioPlugins, dbio_swift_factory(dbioPlugins, "swift", nullptr));

    runConfigStoreTests();
    runGwenGuiTests();
    runStatementTests(installedData);
    runModelTests();
    runInstituteSearchTests(database);

    GWEN_Fini();
    // AB_Banking_new() creates the empty folders ~/.aqbanking/settings and
    // ~/.aqbanking/settings6 while it looks for an old configuration to copy
    // over (under Sailjail, the app may not create them). Files, it must not.
    QStringList written;
    QDirIterator files(home.path(), QDir::Files | QDir::Hidden | QDir::System, QDirIterator::Subdirectories);
    while (files.hasNext()) {
        written.append(QDir(home.path()).relativeFilePath(files.next()));
    }
    Check::that("no file was written to the home folder", written.isEmpty(), written.join(QLatin1Char(',')));
    std::printf("\n%d failure(s)\n", Check::failures);
    return Check::failures;
}
