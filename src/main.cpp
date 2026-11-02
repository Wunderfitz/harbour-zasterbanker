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

#include <QGuiApplication>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickView>
#include <QScopedPointer>
#include <QStandardPaths>

#include <sailfishapp.h>

#include <gcrypt.h>

#include "AccountModel.h"
#include "Banking.h"
#include "InstituteSearch.h"
#include "TanImageProvider.h"

int main(int argc, char *argv[])
{
    QScopedPointer<QGuiApplication> app(SailfishApp::application(argc, argv));
    // Must match OrganizationName and ApplicationName in the .desktop file:
    // Sailjail only lets the app write below ~/.local/share/de.ygriega/zasterbanker.
    app->setOrganizationName(QStringLiteral("de.ygriega"));
    app->setApplicationName(QStringLiteral("zasterbanker"));

    // libgcrypt has to be initialised before its first use, which may be the
    // key generation on the UI thread. Gwenhywfar repeats this later, harmlessly.
    gcry_check_version(nullptr);
    gcry_control(GCRYCTL_INITIALIZATION_FINISHED, 0);

    qmlRegisterUncreatableType<Banking>("de.ygriega.zasterbanker", 1, 0, "Banking",
                                        QStringLiteral("Banking is exposed as the 'banking' context property"));
    qmlRegisterUncreatableType<AccountModel>("de.ygriega.zasterbanker", 1, 0, "AccountModel",
                                             QStringLiteral("Comes from banking.accounts"));

    // Constructed before the view, so they are destroyed after it and QML never
    // sees them go. The engine takes over the image provider and deletes it with
    // the view; Banking only touches it from slots, which no longer run by then.
    TanImageProvider *tanImageProvider = new TanImageProvider();
    Banking banking(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation), tanImageProvider);
    InstituteSearch instituteSearch(SailfishApp::pathTo(QStringLiteral("db/fints_institutes.db")).toLocalFile());

    QScopedPointer<QQuickView> view(SailfishApp::createView());
    view->engine()->addImageProvider(QStringLiteral("tan"), tanImageProvider);
    view->rootContext()->setContextProperty(QStringLiteral("banking"), &banking);
    view->rootContext()->setContextProperty(QStringLiteral("instituteSearch"), &instituteSearch);
    view->setSource(SailfishApp::pathToMainQml());
    view->show();

    return app->exec();
}
