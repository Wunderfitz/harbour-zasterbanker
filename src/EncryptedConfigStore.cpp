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

#include "EncryptedConfigStore.h"

#include <QDataStream>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

#include <gwenhywfar/buffer.h>
#include <gwenhywfar/configmgr.h>
#include <gwenhywfar/db.h>
#include <gwenhywfar/error.h>
#include <gwenhywfar/plugin.h>
#include <gwenhywfar/stringlist.h>

#include <gcrypt.h>

#include <cstdio>
#include <cstring>

/*
 * File layout: magic (4 bytes) | format version (1) | nonce (12) | ciphertext | GCM tag (16).
 * Magic and version are authenticated as additional data, so neither can be
 * altered without the tag failing. A fresh random nonce is used for each write.
 */
namespace {
const char MAGIC[] = { 'Z', 'B', 'C', 'S' };
const char FORMAT_VERSION = 1;
const int HEADER_LENGTH = sizeof(MAGIC) + 1;
const int NONCE_LENGTH = 12;
const int TAG_LENGTH = 16;

EncryptedConfigStore *activeStore = nullptr;

void wipe(QByteArray &data)
{
    if (!data.isEmpty()) {
        // data() detaches, so this overwrites our own copy and not a shared one
        // that is still in use elsewhere.
        std::memset(data.data(), 0, static_cast<size_t>(data.size()));
    }
    data.clear();
}

int GWENHYWFAR_CB getGroupFn(GWEN_CONFIGMGR *, const char *groupName, const char *subGroupName, GWEN_DB_NODE **pDb)
{
    return activeStore ? activeStore->getGroup(groupName, subGroupName, pDb) : GWEN_ERROR_NOT_OPEN;
}

int GWENHYWFAR_CB setGroupFn(GWEN_CONFIGMGR *, const char *groupName, const char *subGroupName, GWEN_DB_NODE *db)
{
    return activeStore ? activeStore->setGroup(groupName, subGroupName, db) : GWEN_ERROR_NOT_OPEN;
}

int GWENHYWFAR_CB hasGroupFn(GWEN_CONFIGMGR *, const char *groupName, const char *subGroupName)
{
    return activeStore ? activeStore->hasGroup(groupName, subGroupName) : GWEN_ERROR_NOT_OPEN;
}

// Only one process ever opens the store, and only its banking thread, so
// locking is bookkeeping AqBanking expects rather than something to enforce.
int GWENHYWFAR_CB lockGroupFn(GWEN_CONFIGMGR *, const char *, const char *)
{
    return 0;
}

int GWENHYWFAR_CB unlockGroupFn(GWEN_CONFIGMGR *, const char *, const char *)
{
    return 0;
}

int GWENHYWFAR_CB getUniqueIdFn(GWEN_CONFIGMGR *, const char *groupName, char *buffer, uint32_t bufferLen)
{
    return activeStore ? activeStore->getUniqueId(groupName, buffer, bufferLen) : GWEN_ERROR_NOT_OPEN;
}

// Same format as Gwenhywfar's "dir" manager. AqBanking only ever asks without
// doCheck, as it keeps its own counters.
int GWENHYWFAR_CB mkUniqueIdFromIdFn(GWEN_CONFIGMGR *, const char *, uint32_t uid, int, char *buffer, uint32_t bufferLen)
{
    if (bufferLen < 9) {
        return GWEN_ERROR_BUFFER_OVERFLOW;
    }
    std::snprintf(buffer, bufferLen, "%08x", uid);
    return 0;
}

int GWENHYWFAR_CB deleteGroupFn(GWEN_CONFIGMGR *, const char *groupName, const char *subGroupName)
{
    return activeStore ? activeStore->deleteGroup(groupName, subGroupName) : GWEN_ERROR_NOT_OPEN;
}

int GWENHYWFAR_CB listGroupsFn(GWEN_CONFIGMGR *, GWEN_STRINGLIST *sl)
{
    return activeStore ? activeStore->listGroups(sl) : GWEN_ERROR_NOT_OPEN;
}

int GWENHYWFAR_CB listSubGroupsFn(GWEN_CONFIGMGR *, const char *groupName, GWEN_STRINGLIST *sl)
{
    return activeStore ? activeStore->listSubGroups(groupName, sl) : GWEN_ERROR_NOT_OPEN;
}

GWEN_CONFIGMGR *GWENHYWFAR_CB factoryFn(GWEN_PLUGIN *, const char *url)
{
    GWEN_CONFIGMGR *mgr = GWEN_ConfigMgr_new(url);
    GWEN_ConfigMgr_SetGetGroupFn(mgr, getGroupFn);
    GWEN_ConfigMgr_SetSetGroupFn(mgr, setGroupFn);
    GWEN_ConfigMgr_SetHasGroupFn(mgr, hasGroupFn);
    GWEN_ConfigMgr_SetLockGroupFn(mgr, lockGroupFn);
    GWEN_ConfigMgr_SetUnlockGroupFn(mgr, unlockGroupFn);
    GWEN_ConfigMgr_SetGetUniqueIdFn(mgr, getUniqueIdFn);
    GWEN_ConfigMgr_SetMkUniqueIdFromIdFn(mgr, mkUniqueIdFromIdFn);
    GWEN_ConfigMgr_SetDeleteGroupFn(mgr, deleteGroupFn);
    GWEN_ConfigMgr_SetListGroupsFn(mgr, listGroupsFn);
    GWEN_ConfigMgr_SetListSubGroupsFn(mgr, listSubGroupsFn);
    return mgr;
}
}

EncryptedConfigStore::EncryptedConfigStore(const QString &filePath, const QByteArray &key)
    : filePath(filePath)
    , key(key)
{
}

EncryptedConfigStore::~EncryptedConfigStore()
{
    if (activeStore == this) {
        activeStore = nullptr;
    }
    wipe(key);
    for (auto group = groups.begin(); group != groups.end(); ++group) {
        for (auto entry = group->begin(); entry != group->end(); ++entry) {
            wipe(entry.value());
        }
    }
}

bool EncryptedConfigStore::dataExists(const QString &filePath)
{
    return QFileInfo(filePath).exists();
}

EncryptedConfigStore::LoadResult EncryptedConfigStore::load()
{
    QFile file(filePath);
    if (!file.exists()) {
        return NoData;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Cannot open" << filePath << file.errorString();
        return ReadFailed;
    }
    const QByteArray sealed = file.readAll();
    file.close();

    QByteArray plain;
    if (!decrypt(sealed, plain)) {
        return DecryptionFailed;
    }

    QDataStream stream(plain);
    stream.setVersion(QDataStream::Qt_5_6);
    stream >> groups >> uniqueIds;
    wipe(plain);
    if (stream.status() != QDataStream::Ok) {
        groups.clear();
        uniqueIds.clear();
        return DecryptionFailed;
    }
    return Loaded;
}

void EncryptedConfigStore::registerAsDirPlugin()
{
    activeStore = this;
    GWEN_PLUGIN_MANAGER *pm = GWEN_PluginManager_FindPluginManager(GWEN_CONFIGMGR_PLUGIN_NAME);
    if (!pm) {
        qWarning() << "Gwenhywfar has no config manager plugin manager";
        return;
    }
    // Asking Gwenhywfar whether "dir" is there would make it try to load the
    // plugin from disk, so the registration is tracked here. A later store in
    // this process (after a reset) just takes over: the factory reads activeStore.
    static bool registered = false;
    if (registered) {
        return;
    }
    registered = true;
    GWEN_PLUGIN *plugin = GWEN_ConfigMgr_Plugin_new(pm, "dir", nullptr);
    GWEN_ConfigMgr_Plugin_SetFactoryFn(plugin, factoryFn);
    GWEN_PluginManager_AddPlugin(pm, plugin);
}

int EncryptedConfigStore::getGroup(const char *groupName, const char *subGroupName, GWEN_DB_NODE **pDb)
{
    QMap<QString, QByteArray> &group = groups[QString::fromUtf8(groupName)];
    const QString subGroup = QString::fromUtf8(subGroupName);
    if (!group.contains(subGroup)) {
        // Gwenhywfar's "dir" manager creates an empty file for a group that is
        // read before it was ever written, and AqBanking relies on that on its
        // first start (e.g. for the backend's own settings). Same here.
        group.insert(subGroup, QByteArray());
        if (!save()) {
            return GWEN_ERROR_IO;
        }
    }
    const auto entry = group.constFind(subGroup);
    GWEN_DB_NODE *db = GWEN_DB_Group_new(subGroupName);
    if (!entry.value().isEmpty()) {
        const int rv = GWEN_DB_ReadFromString(db, entry.value().constData(), entry.value().size(),
                                              GWEN_DB_FLAGS_DEFAULT | GWEN_PATH_FLAGS_CREATE_GROUP);
        if (rv < 0) {
            GWEN_DB_Group_free(db);
            return rv;
        }
    }
    *pDb = db;
    return 0;
}

int EncryptedConfigStore::setGroup(const char *groupName, const char *subGroupName, GWEN_DB_NODE *db)
{
    GWEN_BUFFER *buffer = GWEN_Buffer_new(0, 4096, 0, 1);
    const int rv = GWEN_DB_WriteToBuffer(db, buffer, GWEN_DB_FLAGS_DEFAULT);
    if (rv < 0) {
        GWEN_Buffer_free(buffer);
        return rv;
    }
    QByteArray &entry = groups[QString::fromUtf8(groupName)][QString::fromUtf8(subGroupName)];
    wipe(entry);
    entry = QByteArray(GWEN_Buffer_GetStart(buffer), static_cast<int>(GWEN_Buffer_GetUsedBytes(buffer)));
    std::memset(GWEN_Buffer_GetStart(buffer), 0, GWEN_Buffer_GetUsedBytes(buffer));
    GWEN_Buffer_free(buffer);
    return save() ? 0 : GWEN_ERROR_IO;
}

int EncryptedConfigStore::hasGroup(const char *groupName, const char *subGroupName) const
{
    const auto group = groups.constFind(QString::fromUtf8(groupName));
    if (group != groups.constEnd() && group->contains(QString::fromUtf8(subGroupName))) {
        return 0;
    }
    return GWEN_ERROR_NOT_FOUND;
}

int EncryptedConfigStore::deleteGroup(const char *groupName, const char *subGroupName)
{
    auto group = groups.find(QString::fromUtf8(groupName));
    if (group == groups.end()) {
        return 0;
    }
    auto entry = group->find(QString::fromUtf8(subGroupName));
    if (entry == group->end()) {
        return 0;
    }
    wipe(entry.value());
    group->erase(entry);
    if (group->isEmpty()) {
        groups.erase(group);
    }
    return save() ? 0 : GWEN_ERROR_IO;
}

int EncryptedConfigStore::listGroups(GWEN_STRINGLIST *sl) const
{
    for (auto group = groups.constBegin(); group != groups.constEnd(); ++group) {
        GWEN_StringList_AppendString(sl, group.key().toUtf8().constData(), 0, 1);
    }
    return 0;
}

int EncryptedConfigStore::listSubGroups(const char *groupName, GWEN_STRINGLIST *sl) const
{
    const auto group = groups.constFind(QString::fromUtf8(groupName));
    if (group == groups.constEnd()) {
        return 0;
    }
    for (auto entry = group->constBegin(); entry != group->constEnd(); ++entry) {
        GWEN_StringList_AppendString(sl, entry.key().toUtf8().constData(), 0, 1);
    }
    return 0;
}

int EncryptedConfigStore::getUniqueId(const char *groupName, char *buffer, uint32_t bufferLen)
{
    if (bufferLen < 14) {
        return GWEN_ERROR_BUFFER_OVERFLOW;
    }
    const quint32 uid = ++uniqueIds[QString::fromUtf8(groupName)];
    if (!save()) {
        return GWEN_ERROR_IO;
    }
    std::snprintf(buffer, bufferLen, "uid::%08x", uid);
    return 0;
}

bool EncryptedConfigStore::save()
{
    QByteArray plain;
    {
        QDataStream stream(&plain, QIODevice::WriteOnly);
        stream.setVersion(QDataStream::Qt_5_6);
        stream << groups << uniqueIds;
    }
    const QByteArray sealed = encrypt(plain);
    wipe(plain);
    if (sealed.isEmpty()) {
        return false;
    }

    QDir().mkpath(QFileInfo(filePath).absolutePath());
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Cannot write" << filePath << file.errorString();
        return false;
    }
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    file.write(sealed);
    if (!file.commit()) {
        qWarning() << "Cannot commit" << filePath << file.errorString();
        return false;
    }
    return true;
}

QByteArray EncryptedConfigStore::encrypt(const QByteArray &plain) const
{
    QByteArray sealed(HEADER_LENGTH + NONCE_LENGTH + plain.size() + TAG_LENGTH, '\0');
    char *header = sealed.data();
    std::memcpy(header, MAGIC, sizeof(MAGIC));
    header[sizeof(MAGIC)] = FORMAT_VERSION;
    char *nonce = header + HEADER_LENGTH;
    char *body = nonce + NONCE_LENGTH;
    char *tag = body + plain.size();
    gcry_create_nonce(nonce, NONCE_LENGTH);

    gcry_cipher_hd_t cipher;
    if (gcry_cipher_open(&cipher, GCRY_CIPHER_AES256, GCRY_CIPHER_MODE_GCM, 0)) {
        return QByteArray();
    }
    bool ok = !gcry_cipher_setkey(cipher, key.constData(), static_cast<size_t>(key.size()))
            && !gcry_cipher_setiv(cipher, nonce, NONCE_LENGTH)
            && !gcry_cipher_authenticate(cipher, header, HEADER_LENGTH)
            && !gcry_cipher_encrypt(cipher, body, static_cast<size_t>(plain.size()),
                                    plain.constData(), static_cast<size_t>(plain.size()))
            && !gcry_cipher_gettag(cipher, tag, TAG_LENGTH);
    gcry_cipher_close(cipher);
    if (!ok) {
        qWarning() << "Encryption failed";
        return QByteArray();
    }
    return sealed;
}

bool EncryptedConfigStore::decrypt(const QByteArray &sealed, QByteArray &plain) const
{
    if (sealed.size() < HEADER_LENGTH + NONCE_LENGTH + TAG_LENGTH
            || std::memcmp(sealed.constData(), MAGIC, sizeof(MAGIC)) != 0
            || sealed.at(sizeof(MAGIC)) != FORMAT_VERSION) {
        qWarning() << "Stored data has an unknown format";
        return false;
    }
    const char *header = sealed.constData();
    const char *nonce = header + HEADER_LENGTH;
    const char *body = nonce + NONCE_LENGTH;
    const int bodyLength = sealed.size() - HEADER_LENGTH - NONCE_LENGTH - TAG_LENGTH;
    const char *tag = body + bodyLength;

    plain = QByteArray(bodyLength, '\0');
    gcry_cipher_hd_t cipher;
    if (gcry_cipher_open(&cipher, GCRY_CIPHER_AES256, GCRY_CIPHER_MODE_GCM, 0)) {
        return false;
    }
    bool ok = !gcry_cipher_setkey(cipher, key.constData(), static_cast<size_t>(key.size()))
            && !gcry_cipher_setiv(cipher, nonce, NONCE_LENGTH)
            && !gcry_cipher_authenticate(cipher, header, HEADER_LENGTH)
            && !gcry_cipher_decrypt(cipher, plain.data(), static_cast<size_t>(bodyLength),
                                    body, static_cast<size_t>(bodyLength))
            && !gcry_cipher_checktag(cipher, tag, TAG_LENGTH);
    gcry_cipher_close(cipher);
    if (!ok) {
        qWarning() << "Stored data failed authentication";
        wipe(plain);
    }
    return ok;
}
