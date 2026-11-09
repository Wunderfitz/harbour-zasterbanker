# Zaster Banker

Personal finances for Sailfish OS. Zaster Banker talks directly to German banks via
FinTS (formerly HBCI) with PIN/TAN: it shows your accounts and their balances, the
transactions of the last 90 days and what your depots hold, and it keeps everything it
stores encrypted on your phone.

There is no server in between, no account and no tracking: the app talks to your bank
and to nobody else. FinTS itself is spoken by [AqBanking](https://www.aquamaniac.de/rdm/projects/aqbanking),
the FinTS engine of GnuCash and KMyMoney; Zaster Banker is the Sailfish OS app around
it.

This is the restart of the original [Zaster Banker](https://github.com/Wunderfitz/harbour-zaster),
which ended at version 0.4 around 2019. It shares the name, the icon and the cover
artwork, and continues the version numbers from 0.5 on - but it is a new app with a
new package name, not an update of the old one. There is nothing to migrate.

## What it does

- **Find your bank.** Search as you type, by name, city, bank code or BIC, through the
  FinTS institute list of the Deutsche Kreditwirtschaft that comes with the app.
  Umlauts may be typed or spelled out. Only banks that offer FinTS 3.0 with PIN/TAN
  are listed, and the server address is taken from the list - there is nothing to
  look up or type in yourself.
- **Set up a bank access.** Enter the login of your online banking and the PIN once.
  The app checks the bank's server, does the initial handshake, lets you choose the
  TAN method if the bank offers several, fetches the account list and the balances.
  If any step fails, nothing is left behind.
- **Accounts and balances.** The overview lists the accounts by bank access, each with
  its balance and the date the bank gave it, and a total per bank - one line per
  currency, and a note when the bank did not send a balance for every account. The
  last known balances show as soon as the app starts.
- **Refresh.** All bank accesses from the pull-down menu or the cover, or a single one
  from the context menu of its name (long press), which contacts only that bank and
  asks only for its PIN.
- **Transactions.** Tap an account to see its transactions of the last 90 days,
  pending ones first. Tap a transaction for its full purpose, the IBAN of the other
  side and the kind of transaction; long press to copy it to the clipboard as plain
  text.
- **Depots.** Tap a depot to see what it holds: units, price and date, value per
  position and a total per currency.
- **TANs.** photoTAN and QR-TAN show their coloured code right in the dialog, ready
  for your reading device or app; otherwise the dialog shows the bank's challenge and
  takes the TAN you type.
- **The PIN is remembered for 5 minutes**, per bank access and in memory only, once
  the bank has accepted it - so a refresh right after opening the transactions does
  not ask again. The PIN dialog says so.
- **Cover.** The banks with their totals, and a cover action to refresh them.
- **Remove a bank access**, from the context menu of its name, after a countdown that a
  tap undoes; **"Delete all data"** in the pull-down menu removes everything,
  including the key.

English and German.

## Banks

Every bank in the institute list with a PIN/TAN address and FinTS 3.0 should work, as
the protocol is AqBanking's, which is used with many German banks. What has actually
been tried:

| Bank | TAN method | What was confirmed |
|---|---|---|
| **Volksbank** (Atruvia data centre) | smartTAN photo | The development bank. Setup, balances, transactions, a depot of shares, a photoTAN during a balance fetch, the remembered PIN. |
| **GLS Gemeinschaftsbank** (Atruvia data centre) | not recorded | Setting up the bank access, with its accounts and balances. |
| **EthikBank** (Atruvia data centre) | not recorded | Reported by testers: the current features work. |
| **DKB** (Deutsche Kreditbank, own server) | not recorded | Reported by testers: the current features work. |
| **comdirect** (own server) | not recorded | Reported by a tester: setup and balances work. Transactions did not show up, which is being looked into. |
| **Deutsche Bank**, **Postbank** | BestSign | Reported by testers: setting up the bank access fails after choosing BestSign. Being looked into. |

Three are cooperative banks run by the same data centre, Atruvia; the DKB, comdirect,
Deutsche Bank and Postbank run FinTS servers of their own. Sparkassen, other private banks and other data centres have not been
tried yet, and photoTAN is the only TAN method known to have been used. A report on how the app fares with your bank is welcome, in the
[Telegram group](https://t.me/+K3rZ4wtYd7NjMWMy) or as described in
[Reporting a problem](#reporting-a-problem).

## Installing

**Zaster Banker is in an early testing phase, for brave users.** It will be available
from the Jolla Store, [OpenRepos](https://openrepos.net/) and as a
[GitHub release](https://github.com/Wunderfitz/harbour-zasterbanker/releases) once it
has proven to work with banks across several data centres - so far it has been tried
with banks of Atruvia and with the DKB (see [Banks](#banks)). Until then, the RPMs for testing are
shared in the [testing and development group on Telegram](https://t.me/+K3rZ4wtYd7NjMWMy):
join it if you would like to try the app with your bank, or help with its development.
Install the RPM with `pkcon install-local` or the file manager, after allowing
untrusted software in the Sailfish settings.

Zaster Banker needs **Sailfish OS 4.6** or newer. It passes Harbour's validation (`sfdk
check`) without any exception. Pick the package that matches your phone:

- **Sailfish OS 5.1 and newer:** the 5.1 build for aarch64 or i486. Harbour's 5.1
  tooling rejects binaries built against 4.6, so these are built separately.
- **Sailfish OS 4.6 and 5.0:** the 4.6 build for aarch64 or armv7hl.

It asks for two Sailjail permissions as it starts for the first time:

- **Internet** to talk to your bank.
- **Secrets** to keep the key to its data in Sailfish Secrets.

## Good to know

- **Sailfish Secrets has to work.** The key to the app's data is created there on the
  first start and read on every start after. If Secrets cannot give it out, the app
  says so and does not start banking. If the key is gone while encrypted data exists,
  the data cannot be read anymore; the app tells you and offers "Delete all data" as
  the way out. It never silently replaces a key.
- **Transactions go back 90 days.** Under PSD2, banks generally need no TAN up to that
  age, and most ask for one or refuse anything older.
- **Transactions and holdings are fetched once per account while the app runs.**
  Opening an account again costs no request and no TAN; refresh from the page's
  pull-down menu.
- **Every balance shows its date** - the bank's, or the day it was fetched if the bank
  gave none - so a stored balance does not pass for a current one.
- **A depot's value is units times price.** Bonds and anything else not counted in
  units with a price in a currency show their name and "-", and the total says "not
  all holdings included". The depot's value does not go into the overview's totals.
- **The bank's server must prove its identity.** The certificate is checked against
  the phone's CA bundle and the host name, and there is no "accept anyway".
- **Some texts come from AqBanking**, the PIN and TAN prompts and many of the bank's
  messages among them. They follow the system's language (German or English), not
  the app's.
- **Copying a transaction puts it on the clipboard**, where other apps can read it.

## Privacy

Zaster Banker talks to your bank's FinTS server and to nobody else. There is no server
of its own, no analytics and no crash reporting.

- **What it stores** - your bank accesses with their login names, the accounts, the
  bank's parameters for them and the last balance of each account - is kept in one
  file, `~/.local/share/de.ygriega/zasterbanker/aqbanking.zbs`, sealed with AES-256-GCM
  and readable only by you (mode 0600). AqBanking writes no plain file of its own.
- **The key** is 256 random bits, kept in Sailfish Secrets in a collection only the app
  can open, unlocked together with your device lock. It never touches the file system.
- **Your PIN is never stored.** It is kept in the app's memory for 5 minutes after your
  last action with that bank access, and only once the bank has accepted it. It is
  wiped when the time is up, when the bank rejects it, when you remove the bank access
  or delete all data, and when the app ends.
- **Transactions and holdings are kept in memory only** and are gone when the app ends.
- **Nothing of the conversation with your bank reaches the disk or the system log.**
  AqBanking's message and job logs are compiled out, and release builds log nothing of
  AqBanking or Gwenhywfar at all.
- **The bank's texts are not trusted**: what the bank or the sender of a transfer
  writes is shown as plain text or filtered formatting, so it cannot make the app load
  anything from elsewhere.

## Reporting a problem

Open an [issue](https://github.com/Wunderfitz/harbour-zasterbanker/issues) with the
phone, its Sailfish OS version, your bank (or at least its kind and data centre), the
TAN method and the error the app showed. `journalctl -f` as root while reproducing
shows the app's own warnings; release builds keep AqBanking's own messages to
themselves on purpose. Questions and first impressions are just as welcome in the
[testing and development group on Telegram](https://t.me/+K3rZ4wtYd7NjMWMy).

**Never post your PIN, a TAN, your login, account numbers, IBANs, balances or
transactions**, and check screenshots for them before attaching one.

## Architecture

Zaster Banker is C++ and QML with Silica. AqBanking 6 and its base library Gwenhywfar 5
do the FinTS protocol; the app's own code is the UI, what AqBanking does not offer, and
what makes it fit Sailfish OS: the bank search, the encrypted store, the key in
Sailfish Secrets.

### Threads

```
UI thread                                  banking thread (BankingThread::run)
---------                                  -----------------------------------
QML pages, dialogs, cover                  GWEN_Init(), GwenGui, plugins
Banking  (QML facade, "banking")  --cmd--> command queue, one at a time
KeyStore (Sailfish Secrets)                AqBanking: AB_Banking_*, AqHbciShim
AccountModel, TransactionModel,  <-signal- results, progress, errors
HoldingModel                     <-signal- interactionRequested (PIN, TAN, ...)
PinDialog, TanDialog, ...        --respond-> condition variable wakes the callback
TanImageProvider (image://tan)   <-------- decoded photoTAN/QR image, in memory

InstituteSearch  ------------------------> InstituteSearchWorker (SQLite, read-only)
```

AqBanking is synchronous and not thread-safe. So **one thread owns it**:
`BankingThread`, a `QThread` with `run()` overridden. It is the only thread that ever
calls Gwenhywfar or AqBanking - from `GWEN_Init()` to `GWEN_Fini()` - and it works off
a queue of commands one at a time: initialise, add a bank access, fetch balances (all
or one bank access), fetch transactions, fetch holdings, remove a bank access, reset.
Results go back to the UI thread through queued signals. A FinTS dialog can take many
seconds and wait for a TAN for minutes; the UI never waits with it.

The bank search runs each query in a worker thread with its own read-only SQLite
connection. Only one runs at a time; if the query changed meanwhile, it runs again with
the latest, so keystrokes never queue up.

### Starting up

The order matters, and most of it is dictated by Gwenhywfar and AqBanking:

1. **Process start (`main.cpp`).** `AQHBCI_LOGBOOKED` and `AQHBCI_LOGNOTED` are unset:
   with either set, AqBanking writes raw bank statements to `/tmp`. Organisation and
   application name are set to `de.ygriega` and `zasterbanker`, so `QStandardPaths`
   points where Sailjail allows writing. **libgcrypt is initialised** here
   (`gcry_check_version()`, `GCRYCTL_INITIALIZATION_FINISHED`), as its first use may be
   generating the key on the UI thread, before Gwenhywfar has run; Gwenhywfar repeats
   it later, harmlessly.
2. **The key (`KeyStore`).** Asynchronous requests to Sailfish Secrets on the UI thread
   read the key, or create the collection and the key on the first start. The key goes
   to the banking thread with the `Initialize` command and is wiped from the UI side.
3. **Gwenhywfar (`BankingThread::run()`).** `GWEN_Init()` first. The loggers of
   Gwenhywfar, AqBanking and aqhbci are set to error level. The app's own GUI layer,
   `GwenGui`, is created and installed with `GWEN_Gui_SetGui()` - this is how AqBanking
   will ask for PINs and TANs later. Then **the statement parser is registered by
   hand**: AqBanking's `swift` importer (MT940, MT942, MT535) asks Gwenhywfar for a
   DBIO plugin called `swift`, which upstream is only built as a loadable module. The
   app links it in statically (`swift.a`) and adds `dbio_swift_factory` to Gwenhywfar's
   DBIO plugin manager; without it, every statement would fail to import.
4. **The encrypted store (`EncryptedConfigStore`).** AqBanking keeps its configuration -
   users, accounts, the banks' parameter data, system IDs - through a Gwenhywfar
   *config manager*, and the stock one writes plain text files. The store decrypts its
   file and **registers itself as the config manager plugin named `dir`**, after
   `GWEN_Init()` and **before** `AB_Banking_Init()`. When AqBanking then opens its
   configuration as `dir://<data folder>`, it gets the app's store instead of the
   stock one, without knowing. The store keeps everything in memory and writes it
   back, sealed, on every change. It mirrors the stock manager where AqBanking relies
   on it: a group read before it was ever written exists, empty.
5. **AqBanking.** `AB_Banking_new("ZasterBanker", <data folder>, 0)`, then the FinTS
   product registration number and the app's version go into AqBanking's runtime
   configuration (`fintsRegistrationKey`, `fintsApplicationVersionString`) - both are
   sent to the bank in every dialog - and `AB_Banking_Init()`. AqBanking finds its
   data (`hbci.xml`, the importer profiles, translations) under
   `/usr/share/harbour-zasterbanker/`, a path compiled into the libraries; its two
   backends, `aqhbci` (FinTS) and `aqnone` (a placeholder it cannot start without),
   are built into the static library.
6. **Ready.** The last balances are read from the store and published together with
   the accounts, so the overview fills before the first request to any bank.

Shutting down runs the same way backwards on the banking thread: `AB_Banking_Fini()`
and `AB_Banking_free()`, the store, `GWEN_Gui_SetGui(nullptr)` and the GUI layer (which
wipes all remembered PINs), `GWEN_Fini()`.

### Talking to the bank

AqBanking's public API covers jobs on accounts. A job is an `AB_TRANSACTION` with a
command - `AB_Transaction_CommandGetBalance`, `...GetTransactions`, `...GetDepot` - and
the unique ID of an account. The jobs go into a list, and `AB_Banking_SendCommands()`
does the whole FinTS conversation for them in one call: it opens a dialog with each
bank concerned and logs in with the PIN, sends the jobs, asks for a TAN if the
bank wants one, closes the dialog, and leaves the bank's answers - balances,
statements, securities - in an `AB_IMEXPORTER_CONTEXT`, sorted by account.

**Everything interactive happens in the middle of that call**, through Gwenhywfar's GUI
layer. `GwenGui` implements its callbacks:

| Callback | Used for | What the app does |
|---|---|---|
| `getPassword` | PIN; TAN with the bank's challenge, and for photoTAN/QR-TAN the image in its parameters | Reuses a remembered PIN, or asks the user |
| `setPasswordStatus` | AqBanking reports whether the bank accepted or rejected the PIN | Keeps an accepted PIN for 5 minutes, wipes a rejected one |
| `messageBox` | Questions and notices, decoupled approval in the banking app | Shows the message with its buttons |
| `inputBox` | Other text AqBanking needs | Asks the user |
| `checkCert` | The server certificate, already verified by Gwenhywfar | Accepts it only if verification found nothing at all |
| `progressLog`, `showBox`, `progressStart` | AqBanking's status texts and errors, the bank's messages among them | Shows them as progress; keeps the last error for the result |
| `progressAdvance` | Called regularly during the dialog | Returns "aborted" if the user cancelled |
| `logHook` | Gwenhywfar's and AqBanking's log | Debug builds only, nothing in release builds |

A callback that needs the user calls `BankingThread::requestInteraction()`. That emits
`interactionRequested` to the UI thread - with a TAN image already decoded on the
banking thread and handed to QML through `TanImageProvider` - and then **waits on a
condition variable**, inside AqBanking's call stack, until the QML dialog answers with
`banking.respond()`, the user cancels, or the app shuts down. The callback then returns
the answer to AqBanking, which carries on with the dialog where it stopped. A cancel
returns `GWEN_ERROR_USER_ABORTED`, and AqBanking gives up the job. PIN and TAN are
copied into AqBanking's buffer and wiped from the app's own.

**`AB_Banking_SendCommands()` returning 0 does not mean it worked**: it does so even
when the connection failed, and a bank rejecting a request fails neither the call nor
always the job. So each command checks for itself: the job's status, whether the
context holds an entry for the account (which only an answer from the bank creates),
whether the bank reported an error, and only then takes an empty result as "nothing
there". When a job fails without a word, AqBanking only told its log, which is off; the
app then says whether the job was never sent (the bank's parameters do not offer it for
this account) or the answer could not be read.

Before transactions are fetched, two things are set up that AqBanking would otherwise
get wrong silently:

- **CAMT first.** AqBanking only tries MT940 unless the account carries the flag
  `AH_BANK_FLAGS_PREFER_CAMT_DOWNLOAD`, and creates the MT940 job only if the bank's
  parameters list it - many banks today only offer CAMT. The app sets the flag, and
  AqBanking falls back to MT940 by itself where CAMT is not offered. The `xml`
  importer's `camt_*.conf` profiles are installed for this, and are not optional:
  AqBanking derives the formats it offers the bank from their file names.
- **The account's BIC.** CAMT requests identify the account by IBAN and BIC, but the
  account list does not bring the BIC. If it is missing, the app fetches the SEPA
  details of the account (`HKSPA`) first.

### Setting up a bank access

AqBanking's public API has no way to create a FinTS user; that is only offered by the
"control" commands of its `aqhbci-tool4`, which print to the terminal. `AqHbciShim.c`
does what those commands do - `adduser`, `getsysid`, `getaccounts`, `listitanmodes`,
`setitanmode`, `deluser` - line by line after them, but returns structured results. It
uses aqhbci's internal headers from AqBanking's source tree, which works because
AqBanking is linked statically, and which means the shim has to be compared with the
control commands whenever AqBanking is upgraded.

Setting up is one command on the banking thread, so the PIN is entered once:

1. **Add the user**: a PIN/TAN user for FinTS 3.0, with bank code, login and the server
   address from the bank list.
2. **The handshake**: fetch and check the server's certificate, then the system ID. In
   its answer the bank reports which TAN methods it allows this user.
3. **The TAN method**: chosen by the user if there is more than one, set without asking
   if there is one, and none at all if the bank needs no two-step TAN.
4. **The accounts**: the account list, which AqBanking adds to its store.
5. **The balances** of the new bank access's accounts only, so no other bank is
   contacted and no other PIN asked for. Balances that do not come are no reason to
   undo the setup.

If any step but the last fails, the half-made user is deleted again with its accounts:
nothing is left behind.

## Libraries

Linked from the system, all on Harbour's allowed list: Qt 5 (Core, Gui, Qml, Quick,
Sql), libsailfishapp, Silica and libsailfishsecrets.

Everything else is not on that list, and GnuTLS is not even installed on the phone. So
`3rdparty/build-deps.sh` builds these seven as static, position-independent archives,
and the app links them in; the binary does not depend on any of them at run time.

| Library | Version | Licence | What for |
|---|---|---|---|
| [AqBanking](https://www.aquamaniac.de/rdm/projects/aqbanking) | 6.9.2 | GPLv2 or GPLv3 | FinTS: dialogs, jobs, TAN handling, statement import (MT940/942, CAMT, MT535) |
| [Gwenhywfar](https://www.aquamaniac.de/rdm/projects/gwenhywfar) | 5.14.1 | LGPL-2.1+ | AqBanking's base: HTTPS, configuration, GUI callbacks, plugins, parsers |
| [GnuTLS](https://www.gnutls.org/) | 3.8.13 | LGPL-2.1+ | TLS for Gwenhywfar |
| [Nettle](https://www.lysator.liu.se/~nisse/nettle/) | 3.10.2 | LGPLv3+ or GPLv2+ | Cryptography for GnuTLS |
| [GMP](https://gmplib.org/) | 6.3.0 | LGPLv3+ or GPLv2+ | Big numbers for Nettle and GnuTLS |
| [libgcrypt](https://gnupg.org/software/libgcrypt/) | 1.12.4 | LGPL-2.1+ | Cryptography for Gwenhywfar; the app's AES-256-GCM and key generation |
| [libgpg-error](https://gnupg.org/software/libgpg-error/) | 1.61 | LGPL-2.1+ | Error codes for libgcrypt |

Versions are pinned in the script with SHA-256 checksums, taken after verifying the
upstream signatures. The configuration is kept small: no assembly in GMP, GnuTLS
without p11-kit, IDN, compression or TPM and trusting the phone's CA bundle, Gwenhywfar
without GUIs and without any loadable plugins, AqBanking with the `aqhbci` and `aqnone`
backends only. Gwenhywfar and AqBanking are built with translations (glibc's gettext),
compiled from their German catalogues by `tools/po2mo.py`, as the build engine has no
`msgfmt`.

Four patches in `3rdparty/patches/` are applied on top:

- **AqBanking: no logs on disk.** AqBanking logs every FinTS message and every job to
  plain files in its data folder. Both are compiled out.
- **AqBanking: depot answers.** `hbci.xml` defines the depot statement (`HIWPD`)
  version 6 twice, and a version 6 answer was decoded as the wrong one, so fetching a
  depot failed. It now reads both.
- **AqBanking: depot prices.** The MT535 import lost every price, dated each the 1st of
  its month and dropped umlauts in names where `char` is signed.
- **Gwenhywfar: a certificate is required.** Its HTTPS client carried on with a server
  that presented no certificate at all.

## Building

You need the Sailfish SDK, and two things that are not in this repository:

- **A FinTS product registration number.** Every FinTS application has to send one;
  you get it by registering your application with the
  [Deutsche Kreditwirtschaft](https://www.hbci-zka.de/register/prod_register.htm). Copy
  `src/FinTsRegistration.h.example` to `src/FinTsRegistration.h` and enter it. That
  file is git-ignored; the number is compiled into the binary and must never be
  committed.
- **The FinTS institute list.** Registered developers receive it from the Deutsche
  Kreditwirtschaft as a CSV export. Its licence does not allow publishing it, so it is
  not in Git; `tools/import-banklist.py` turns it into `db/fints_institutes.db`, which
  is packaged into the RPM:

  ```sh
  tools/import-banklist.py "fints_institute NEU mit BIC Master.csv"
  ```

Then, out of tree as Qt Creator does it, one build directory per target. First the
third-party libraries, once per architecture, inside the build engine - roughly half an
hour each. `build-shell` needs an initialised build directory:

```sh
mkdir -p ../build-harbour-zasterbanker-4.6.0.13-aarch64
cd ../build-harbour-zasterbanker-4.6.0.13-aarch64
sfdk -c target=SailfishOS-4.6.0.13-aarch64 build-init ../harbour-zasterbanker
sfdk -c target=SailfishOS-4.6.0.13-aarch64 build-shell ../harbour-zasterbanker/3rdparty/build-deps.sh
```

The script downloads into `3rdparty/downloads/`, builds in `3rdparty/build/<arch>/` and
installs into `3rdparty/install/<arch>/`, all git-ignored, and continues where it
stopped if run again. The libraries built against 4.6 serve the 5.1 builds of the same
architecture as well. Then the app:

```sh
sfdk -c target=SailfishOS-4.6.0.13-aarch64 build ../harbour-zasterbanker
sfdk -c target=SailfishOS-4.6.0.13-aarch64 check RPMS/*.rpm
```

The RPMs end up in `RPMS/` inside the build directory. The build engine only sees your
home folder, so the build directories must be below it, never under `/tmp`. Builds
target the oldest supported release, 4.6.0.13 (Qt 5.6.3), and 5.1.0.11 for the 5.1
packages (see [Installing](#installing)).

### Tests

The tests of the tools need nothing but Python 3:

```sh
python3 -m unittest discover -s tests/host -v
```

The others link the app's own code with AqBanking, Gwenhywfar and Qt as the app has
them: the encrypted store, the PIN memory and the filter on the bank's texts, sample
statements in MT940, MT942, CAMT and MT535 read by AqBanking's real importers, the
models and the bank search. They run in the build engine, from the i486 build
directory, once the libraries are built for it:

```sh
cd ../build-harbour-zasterbanker-4.6.0.13-i486
sfdk -c target=SailfishOS-4.6.0.13-i486 build-shell ../harbour-zasterbanker/tests/run-engine-tests.sh
```

Every push to master runs all of them on GitHub, builds the four packages, and checks
each with Harbour's validation; a tag on master makes a release. The libraries are
built there once and kept in GitHub's cache.

[CLAUDE.md](CLAUDE.md) has every design decision with the reason behind it, the build
engine's quirks, how to run the app in the emulator, and what has been verified where.

## Layout

```
harbour-zasterbanker.pro   one qmake project; includes 3rdparty/aqbanking.pri
src/                       C++: banking thread, GUI layer, encrypted store, key store,
                           the shim to aqhbci, statement reader, models, bank search
qml/                       the pages, the dialogs, the cover and the components
3rdparty/                  build-deps.sh, the patches and aqbanking.pri; downloads,
                           builds and installs of the libraries (git-ignored)
tools/                     import-banklist.py (bank list), po2mo.py (translations)
db/                        fints_institutes.db, generated (git-ignored)
images/                    the cover artwork
icons/                     the launcher icon
translations/              the German catalogue
tests/                     host/ (the tools), engine/ (the rest, in the build engine),
                           data/ (a made-up bank list), search-focus.sh (emulator)
rpm/                       package spec and changelog
.github/workflows/         the nightly and release builds
CLAUDE.md                  how the app is put together and why
```

## Not there yet

- **More TAN methods.** chipTAN optical (the flicker code) is not shown yet, nor
  chipTAN USB. Methods where you type a TAN from another device or app (pushTAN,
  SMS) use the same dialog as photoTAN, and decoupled approval in the banking app comes
  as a message AqBanking asks the app to show - both should work, but neither has been
  tried with a real bank.
- **Transfers**, standing orders and everything else that sends money.
- **Bond values** in depots (see [Good to know](#good-to-know)).
- **Beyond banking**: Zaster Banker is meant to grow into a personal finance app.

## Licence

Zaster Banker is GPLv3 or later - see [LICENSE](LICENSE) for the licence text, and the
header on each source file for the grant.

The statically linked libraries keep their own licences (see [Libraries](#libraries)):
GPLv2 or GPLv3 for AqBanking, LGPL for the others. All of them are compatible with the
app's GPLv3 or later and with static linking, as the app's sources are public.

## Credits

- FinTS is spoken by [AqBanking](https://www.aquamaniac.de/rdm/projects/aqbanking) and
  Gwenhywfar by Martin Preuß and contributors. Secure connections use GnuTLS, Nettle
  and GMP; encryption uses libgcrypt and libgpg-error. Thanks to all their authors!
- The list of banks is the FinTS institute list of the Deutsche Kreditwirtschaft.
- The icon and the cover artwork are those of the original Zaster Banker.
- Written by Sebastian J. Wolf [sebastian@ygriega.de](mailto:sebastian@ygriega.de)
  together with Claude, Anthropic's assistant, in
  [Claude Code](https://claude.com/claude-code).
