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

#ifndef ENCRYPTEDCONFIGSTORE_H
#define ENCRYPTEDCONFIGSTORE_H

#include <QByteArray>
#include <QMap>
#include <QString>

// Unlike Gwenhywfar's other headers, this one has no extern "C" guard.
extern "C" {
#include <gwenhywfar/configmgr_be.h>
}

/**
 * AqBanking's whole configuration - bank accesses, user and customer IDs, the
 * banks' parameter data, accounts - kept in one AES-256-GCM encrypted file.
 *
 * AqBanking reads and writes its configuration through a Gwenhywfar "config
 * manager". The stock one, "dir", writes plain text files. AqBanking always asks
 * for "dir://...", so registerAsDirPlugin() puts this store into Gwenhywfar's
 * plugin list under that name before AqBanking starts; Gwenhywfar looks there
 * before it would ever load a plugin from disk (and the app ships none).
 *
 * The contents are held in memory as serialised GWEN_DB groups; every change
 * rewrites the encrypted file atomically. Only the banking thread uses it.
 *
 * The app keeps a few things of its own in the same file (appData()), in a
 * group AqBanking neither knows nor gets to see: they are protected exactly
 * like AqBanking's data and go with it when all data is deleted.
 */
class EncryptedConfigStore
{
public:
    enum LoadResult {
        Loaded,
        NoData,
        DecryptionFailed,
        ReadFailed
    };

    EncryptedConfigStore(const QString &filePath, const QByteArray &key);
    ~EncryptedConfigStore();

    LoadResult load();

    /** Makes this store the one AqBanking uses. Call after GWEN_Init(), before AB_Banking_Init(). */
    void registerAsDirPlugin();

    static bool dataExists(const QString &filePath);

    int getGroup(const char *groupName, const char *subGroupName, GWEN_DB_NODE **pDb);
    int setGroup(const char *groupName, const char *subGroupName, GWEN_DB_NODE *db);
    int hasGroup(const char *groupName, const char *subGroupName) const;
    int deleteGroup(const char *groupName, const char *subGroupName);
    int listGroups(GWEN_STRINGLIST *sl) const;
    int listSubGroups(const char *groupName, GWEN_STRINGLIST *sl) const;
    int getUniqueId(const char *groupName, char *buffer, uint32_t bufferLen);

    /** The app's own data stored under the given name, empty if there is none. */
    QByteArray appData(const QString &name) const;
    /** Stores the app's own data under the given name, replacing what was there. */
    bool setAppData(const QString &name, const QByteArray &data);

private:
    bool save();
    QByteArray encrypt(const QByteArray &plain) const;
    bool decrypt(const QByteArray &sealed, QByteArray &plain) const;

    QString filePath;
    QByteArray key;
    QMap<QString, QMap<QString, QByteArray> > groups;
    QMap<QString, quint32> uniqueIds;
};

#endif // ENCRYPTEDCONFIGSTORE_H
