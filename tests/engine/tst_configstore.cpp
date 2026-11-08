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
 * EncryptedConfigStore: what is sealed, how, and the "dir" config manager's
 * behaviour AqBanking relies on.
 */

#include "Check.h"
#include "EncryptedConfigStore.h"

#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include <gwenhywfar/db.h>
#include <gwenhywfar/error.h>
#include <gwenhywfar/stringlist.h>

#include <gcrypt.h>

namespace {

QByteArray testKey(char fill)
{
    return QByteArray(32, fill);
}

QByteArray readFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

void writeFile(const QString &path, const QByteArray &data)
{
    QFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(data);
    }
}

GWEN_DB_NODE *userGroup(const char *name)
{
    GWEN_DB_NODE *db = GWEN_DB_Group_new("user");
    GWEN_DB_SetCharValue(db, GWEN_DB_FLAGS_OVERWRITE_VARS, "userName", name);
    GWEN_DB_SetIntValue(db, GWEN_DB_FLAGS_OVERWRITE_VARS, "hbciVersion", 300);
    return db;
}

QString userName(EncryptedConfigStore &store, const char *group, const char *subGroup)
{
    GWEN_DB_NODE *db = nullptr;
    if (store.getGroup(group, subGroup, &db) < 0 || !db) {
        return QStringLiteral("(not readable)");
    }
    const QString name = QString::fromUtf8(GWEN_DB_GetCharValue(db, "userName", 0, ""));
    GWEN_DB_Group_free(db);
    return name;
}

QStringList listOf(GWEN_STRINGLIST *list)
{
    QStringList result;
    for (GWEN_STRINGLISTENTRY *entry = GWEN_StringList_FirstEntry(list); entry;
         entry = GWEN_StringListEntry_Next(entry)) {
        result.append(QString::fromUtf8(GWEN_StringListEntry_Data(entry)));
    }
    GWEN_StringList_free(list);
    return result;
}

/*
 * The file format as documented, decrypted independently of the store: magic
 * and version, nonce, ciphertext, tag, with magic and version as authenticated
 * data. Returns an empty array if it does not authenticate.
 */
QByteArray openSealed(const QByteArray &sealed, const QByteArray &key)
{
    const int header = 5;
    const int nonce = 12;
    const int tag = 16;
    if (sealed.size() < header + nonce + tag) {
        return QByteArray();
    }
    const int length = sealed.size() - header - nonce - tag;
    QByteArray plain(length, '\0');
    gcry_cipher_hd_t cipher;
    if (gcry_cipher_open(&cipher, GCRY_CIPHER_AES256, GCRY_CIPHER_MODE_GCM, 0)) {
        return QByteArray();
    }
    const bool ok = !gcry_cipher_setkey(cipher, key.constData(), key.size())
            && !gcry_cipher_setiv(cipher, sealed.constData() + header, nonce)
            && !gcry_cipher_authenticate(cipher, sealed.constData(), header)
            && !gcry_cipher_decrypt(cipher, plain.data(), length, sealed.constData() + header + nonce, length)
            && !gcry_cipher_checktag(cipher, sealed.constData() + header + nonce + length, tag);
    gcry_cipher_close(cipher);
    return ok ? plain : QByteArray();
}

}

void runConfigStoreTests()
{
    Check::section("EncryptedConfigStore");
    QTemporaryDir folder;
    const QString path = folder.path() + QStringLiteral("/sub/aqbanking.zbs");

    {
        EncryptedConfigStore store(path, testKey('k'));
        Check::that("no file is no data", store.load() == EncryptedConfigStore::NoData);
        Check::that("no file before the first write", !QFile::exists(path));

        GWEN_DB_NODE *db = userGroup("Volksbank Geheimstadt");
        Check::that("a group is written", store.setGroup("users", "00000001", db) == 0);
        GWEN_DB_Group_free(db);
        Check::that("the folder is created", QFile::exists(path));
        Check::that("readable and writable by the owner only",
                    QFileInfo(path).permissions() == (QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                                      | QFileDevice::ReadUser | QFileDevice::WriteUser),
                    QString::number(QFileInfo(path).permissions(), 16));
        Check::that("the store reads its own group back", userName(store, "users", "00000001") == QStringLiteral("Volksbank Geheimstadt"));
        Check::that("app data is stored", store.setAppData(QStringLiteral("balances"), QByteArray("1234,56 EUR")));
    }

    const QByteArray sealed = readFile(path);
    Check::that("the file starts with magic and version", sealed.startsWith(QByteArray("ZBCS\x01", 5)));
    Check::that("nothing is readable in the file",
                !sealed.contains("Geheimstadt") && !sealed.contains("1234,56") && !sealed.contains("users"));
    const QByteArray opened = openSealed(sealed, testKey('k'));
    Check::that("the file is AES-256-GCM as documented", opened.contains("Geheimstadt") && opened.contains("1234,56"));

    {
        EncryptedConfigStore store(path, testKey('k'));
        Check::that("the same key loads it", store.load() == EncryptedConfigStore::Loaded);
        Check::equal("a group survives", userName(store, "users", "00000001"), QStringLiteral("Volksbank Geheimstadt"));
        Check::equal("app data survives", QString::fromUtf8(store.appData(QStringLiteral("balances"))), QStringLiteral("1234,56 EUR"));
        Check::that("an existing group is there", store.hasGroup("users", "00000001") == 0);
        Check::that("a group not written is not there", store.hasGroup("users", "00000002") == GWEN_ERROR_NOT_FOUND);

        // Gwenhywfar's "dir" manager creates a group that is read before it was
        // written, and AqBanking relies on it on its first start.
        GWEN_DB_NODE *db = nullptr;
        Check::that("a group never written reads as empty",
                    store.getGroup("backends", "aqhbci", &db) == 0 && db && !GWEN_DB_GetFirstVar(db) && !GWEN_DB_GetFirstGroup(db));
        GWEN_DB_Group_free(db);
        Check::that("and exists from then on", store.hasGroup("backends", "aqhbci") == 0);

        const QStringList groups = listOf([&store]() { GWEN_STRINGLIST *l = GWEN_StringList_new(); store.listGroups(l); return l; }());
        Check::that("AqBanking sees its groups", groups.contains(QStringLiteral("users")) && groups.contains(QStringLiteral("backends")),
                    groups.join(QLatin1Char(',')));
        Check::that("but not the app's own", !groups.contains(QStringLiteral("zasterbanker")), groups.join(QLatin1Char(',')));
        const QStringList appSubGroups = listOf([&store]() { GWEN_STRINGLIST *l = GWEN_StringList_new(); store.listSubGroups("zasterbanker", l); return l; }());
        Check::that("nor what is in it", appSubGroups.isEmpty(), appSubGroups.join(QLatin1Char(',')));

        char first[32];
        char second[32];
        Check::that("unique IDs count up per group",
                    store.getUniqueId("users", first, sizeof(first)) == 0 && store.getUniqueId("users", second, sizeof(second)) == 0
                    && QByteArray(first) == "uid::00000001" && QByteArray(second) == "uid::00000002",
                    QString::fromLatin1(first) + QLatin1Char(' ') + QString::fromLatin1(second));

        Check::that("a group is deleted", store.deleteGroup("users", "00000001") == 0 && store.hasGroup("users", "00000001") == GWEN_ERROR_NOT_FOUND);
    }

    {
        EncryptedConfigStore store(path, testKey('k'));
        store.load();
        char id[32];
        Check::that("unique IDs survive", store.getUniqueId("users", id, sizeof(id)) == 0 && QByteArray(id) == "uid::00000003",
                    QString::fromLatin1(id));
        Check::that("deletions survive", store.hasGroup("users", "00000001") == GWEN_ERROR_NOT_FOUND);
    }

    const QByteArray before = readFile(path);
    {
        EncryptedConfigStore store(path, testKey('k'));
        store.load();
        store.setAppData(QStringLiteral("balances"), QByteArray("1234,56 EUR"));
        store.setAppData(QStringLiteral("balances"), QByteArray("1234,56 EUR"));
    }
    const QByteArray after = readFile(path);
    Check::that("each write takes a fresh nonce", before.mid(5, 12) != after.mid(5, 12));

    {
        EncryptedConfigStore store(path, testKey('x'));
        Check::that("another key is refused", store.load() == EncryptedConfigStore::DecryptionFailed);
    }

    const QByteArray good = readFile(path);
    for (int position : { 4, 10, 30, good.size() - 1 }) {
        QByteArray damaged = good;
        damaged[position] = static_cast<char>(damaged.at(position) ^ 0x01);
        writeFile(path, damaged);
        EncryptedConfigStore store(path, testKey('k'));
        const QByteArray name = "a changed byte is refused (at " + QByteArray::number(position) + ")";
        Check::that(name.constData(), store.load() == EncryptedConfigStore::DecryptionFailed);
    }
    writeFile(path, good.left(20));
    {
        EncryptedConfigStore store(path, testKey('k'));
        Check::that("a cut-off file is refused", store.load() == EncryptedConfigStore::DecryptionFailed);
    }
}
