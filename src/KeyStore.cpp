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

#include "KeyStore.h"

#include <QDebug>

#include <Sailfish/Secrets/collectionnamesrequest.h>
#include <Sailfish/Secrets/createcollectionrequest.h>
#include <Sailfish/Secrets/deletecollectionrequest.h>
#include <Sailfish/Secrets/result.h>
#include <Sailfish/Secrets/secret.h>
#include <Sailfish/Secrets/storedsecretrequest.h>
#include <Sailfish/Secrets/storesecretrequest.h>

#include <gcrypt.h>

using namespace Sailfish::Secrets;

namespace {
// Collection names are global to the secrets daemon, hence the app's name in it.
const QString COLLECTION_NAME = QStringLiteral("ZasterBanker");
const QString KEY_NAME = QStringLiteral("DataKey");
const int KEY_LENGTH = 32;

Secret::Identifier keyIdentifier()
{
    return Secret::Identifier(KEY_NAME, COLLECTION_NAME, SecretManager::DefaultEncryptedStoragePluginName);
}
}

KeyStore::KeyStore(QObject *parent)
    : QObject(parent)
    , pendingRequest(nullptr)
    , step(Idle)
    , dataExists(false)
{
}

void KeyStore::requestKey(bool dataExists)
{
    if (step != Idle) {
        qWarning() << "KeyStore busy, ignoring key request";
        return;
    }
    this->dataExists = dataExists;
    CollectionNamesRequest *request = new CollectionNamesRequest();
    request->setStoragePluginName(SecretManager::DefaultEncryptedStoragePluginName);
    startStep(ListingCollections, request);
}

void KeyStore::deleteKey()
{
    if (step != Idle) {
        qWarning() << "KeyStore busy, ignoring delete request";
        return;
    }
    DeleteCollectionRequest *request = new DeleteCollectionRequest();
    request->setCollectionName(COLLECTION_NAME);
    request->setStoragePluginName(SecretManager::DefaultEncryptedStoragePluginName);
    request->setUserInteractionMode(SecretManager::SystemInteraction);
    startStep(DeletingCollection, request);
}

void KeyStore::startStep(Step step, Request *request)
{
    this->step = step;
    pendingRequest = request;
    request->setParent(this);
    request->setManager(&secretManager);
    connect(request, &Request::statusChanged, this, [this, request]() {
        if (request == pendingRequest && request->status() == Request::Finished) {
            handleFinished();
        }
    });
    request->startRequest();
}

void KeyStore::handleFinished()
{
    Request *request = pendingRequest;
    pendingRequest = nullptr;
    request->deleteLater();
    const Result result = request->result();
    const bool succeeded = result.code() == Result::Succeeded;
    if (!succeeded) {
        qWarning() << "Secrets request failed in step" << step << result.errorCode() << result.errorMessage();
    }

    switch (step) {
    case ListingCollections: {
        if (!succeeded) {
            fail(tr("Sailfish Secrets is not available: %1").arg(result.errorMessage()));
            return;
        }
        const bool collectionExists = static_cast<CollectionNamesRequest *>(request)->collectionNames().contains(COLLECTION_NAME);
        if (collectionExists) {
            StoredSecretRequest *read = new StoredSecretRequest();
            read->setIdentifier(keyIdentifier());
            read->setUserInteractionMode(SecretManager::SystemInteraction);
            startStep(ReadingKey, read);
        } else if (dataExists) {
            fail(tr("The key to your stored data is gone, the data cannot be decrypted anymore."), true);
        } else {
            CreateCollectionRequest *create = new CreateCollectionRequest();
            create->setCollectionName(COLLECTION_NAME);
            create->setCollectionLockType(CreateCollectionRequest::DeviceLock);
            create->setDeviceLockUnlockSemantic(SecretManager::DeviceLockKeepUnlocked);
            create->setAccessControlMode(SecretManager::OwnerOnlyMode);
            create->setUserInteractionMode(SecretManager::SystemInteraction);
            create->setStoragePluginName(SecretManager::DefaultEncryptedStoragePluginName);
            create->setEncryptionPluginName(SecretManager::DefaultEncryptedStoragePluginName);
            create->setAuthenticationPluginName(SecretManager::DefaultAuthenticationPluginName);
            startStep(CreatingCollection, create);
        }
        break;
    }
    case ReadingKey: {
        const QByteArray key = succeeded ? static_cast<StoredSecretRequest *>(request)->secret().data() : QByteArray();
        if (key.size() == KEY_LENGTH) {
            step = Idle;
            emit keyAvailable(key);
        } else if (!dataExists) {
            // A collection without a usable key and no data to protect: a first
            // start that was interrupted before the key was stored. Start over.
            createKey();
        } else if (succeeded) {
            fail(tr("The key to your stored data is damaged."), true);
        } else if (notConfirmed(result)) {
            fail(tr("After every restart of the device, Sailfish OS asks whether Zaster Banker may read the "
                    "key to your stored data. This time it was not confirmed (%1).").arg(result.errorMessage()));
        } else {
            fail(tr("The key to your stored data could not be read: %1").arg(result.errorMessage()));
        }
        break;
    }
    case CreatingCollection:
        if (!succeeded) {
            fail(tr("The key store could not be created: %1").arg(result.errorMessage()));
            return;
        }
        createKey();
        break;
    case StoringKey:
        if (!succeeded) {
            newKey.fill('\0');
            newKey.clear();
            fail(tr("The key could not be stored: %1").arg(result.errorMessage()));
            return;
        } else {
            const QByteArray key = newKey;
            newKey.fill('\0');
            newKey.clear();
            step = Idle;
            emit keyAvailable(key);
        }
        break;
    case DeletingCollection:
        step = Idle;
        if (succeeded || result.errorCode() == Result::InvalidCollectionError) {
            emit keyDeleted(true, QString());
        } else if (notConfirmed(result)) {
            emit keyDeleted(false, tr("Sailfish OS asks to confirm the removal of the key, and it was not "
                                      "confirmed (%1).").arg(result.errorMessage()));
        } else {
            emit keyDeleted(false, result.errorMessage());
        }
        break;
    case Idle:
        break;
    }
}

void KeyStore::createKey()
{
    newKey.resize(KEY_LENGTH);
    gcry_randomize(newKey.data(), KEY_LENGTH, GCRY_VERY_STRONG_RANDOM);

    Secret secret(keyIdentifier());
    secret.setType(Secret::TypeBlob);
    secret.setData(newKey);

    StoreSecretRequest *store = new StoreSecretRequest();
    store->setSecretStorageType(StoreSecretRequest::CollectionSecret);
    store->setUserInteractionMode(SecretManager::SystemInteraction);
    store->setSecret(secret);
    startStep(StoringKey, store);
}

// The ways Sailfish OS's request to the user ends without a confirmation: cancelled,
// a wrong security code or polkit refusing otherwise, no answer in time.
bool KeyStore::notConfirmed(const Result &result)
{
    switch (result.errorCode()) {
    case Result::InteractionViewUserCanceledError:
    case Result::IncorrectAuthenticationCodeError:
    case Result::AuthenticationTimeoutError:
    case Result::InteractionViewError:
        return true;
    default:
        return false;
    }
}

void KeyStore::fail(const QString &message, bool keyMissing)
{
    step = Idle;
    emit keyFailed(message, keyMissing);
}
