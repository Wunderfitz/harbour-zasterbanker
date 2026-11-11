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

#ifndef KEYSTORE_H
#define KEYSTORE_H

#include <QByteArray>
#include <QObject>
#include <QString>

#include <Sailfish/Secrets/secretmanager.h>

namespace Sailfish {
namespace Secrets {
class Request;
class Result;
}
}

/**
 * Keeps the app's data key in Sailfish Secrets.
 *
 * Everything Zaster Banker writes to disk is encrypted with one random 256-bit
 * key (see EncryptedConfigStore). The key never touches the file system: it
 * lives in an app-owned collection of the secrets daemon's encrypted storage,
 * which unlocks together with the device lock.
 *
 * The daemon is talked to asynchronously, on the UI thread, which is fine as
 * every step is a short D-Bus round trip that never blocks the event loop.
 *
 * The collection is locked again whenever the daemon restarts, i.e. after every
 * reboot. Reading the key, or deleting the collection, then makes Sailfish OS ask
 * the user to confirm, with the security code if the device has one. Anything
 * but a confirmation fails the request.
 */
class KeyStore : public QObject
{
    Q_OBJECT

public:
    explicit KeyStore(QObject *parent = nullptr);

    /**
     * Fetches the key, creating it on the first start. dataExists says whether
     * encrypted data is already on disk: a key missing then is reported as such
     * and never silently replaced, as a new key would orphan that data.
     */
    void requestKey(bool dataExists);

    /**
     * Removes the key and its collection, which makes the stored data unreadable for good.
     * If the collection is locked, Sailfish OS asks the user to confirm first.
     */
    void deleteKey();

signals:
    void keyAvailable(const QByteArray &key);
    void keyFailed(const QString &message, bool keyMissing);
    void keyDeleted(bool success, const QString &message);

private:
    enum Step {
        Idle,
        ListingCollections,
        ReadingKey,
        CreatingCollection,
        StoringKey,
        DeletingCollection
    };

    void startStep(Step step, Sailfish::Secrets::Request *request);
    void handleFinished();
    void createKey();
    void fail(const QString &message, bool keyMissing = false);
    static bool notConfirmed(const Sailfish::Secrets::Result &result);

    Sailfish::Secrets::SecretManager secretManager;
    Sailfish::Secrets::Request *pendingRequest;
    Step step;
    bool dataExists;
    QByteArray newKey;
};

#endif // KEYSTORE_H
