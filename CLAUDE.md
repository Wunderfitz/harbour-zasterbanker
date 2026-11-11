# harbour-zasterbanker

Zaster Banker for Sailfish OS, restarted. A personal finance app, built
**iteratively**, that contains at its core a banking client for Germany's
FinTS (formerly HBCI) standard and is meant to grow beyond it.

It is the spiritual successor of the original Zaster Banker
(`~/git/harbour-zaster`, versions up to 0.4, stopped around 2019). It shares
the user-facing name, the icon and the cover artwork - nothing else. The
package name differs (`harbour-zasterbanker` instead of `harbour-zaster`), and
there is **no upgrade path** from the old app on purpose. Versions continue the
old line: this project starts at **0.5**.

Iteration 1 (version 0.5, the proof of concept): choose a bank with search as
you type, enter the login, do the initial handshake with the bank (system ID,
TAN method, account list) and show the accounts with their current balances.
photoTAN is the TAN method this iteration is tested with.

Iteration 2 (decided 2026-11-05: the proof of concept is over): the transactions
of an account, on a page of their own, opened by tapping the account.

## Ground rules

Set by the project owner at the start; they apply to every iteration.

- **C++ and QML/JavaScript with Silica**, licensed **GPLv3 or later**. Every
  file under `src/`, `qml/` and `tools/` carries the GPL header block; new
  files get the same one. The spec says `License: GPLv3+`.
- **Style follows the author's other apps**: harbour-lauscher first (the most
  recent, and the closest in idiom), then harbour-chronograph,
  harbour-fernschreiber and harbour-piepmatz, all under `~/git`. Look there
  before inventing a pattern - pages, about page, comments, naming, `.pro`
  and spec layout come from them.
- **Sailfish OS 4.6 or newer** (decided 2026-11-02). Build against the oldest
  supported target, `SailfishOS-4.6.0.13`, which is Qt 5.6.3: no Qt API newer
  than that. Third-party libraries are built against it as well.
- **Harbour-compliant.** Any deviation is asked for first and then recorded
  under [Harbour exceptions](#harbour-exceptions). Do not add one without asking.
- **Patch AqBanking and the other third-party libraries only as a last
  resort** (project owner, 2026-11-11). When unsure how to fix or implement
  something, look first at how GnuCash and KMyMoney, both built on AqBanking,
  and their documentation handle it.
- **Do not implement the FinTS protocol ourselves.** AqBanking does the heavy
  lifting (see [FinTS library](#fints-library-aqbanking)). Our own code is the
  UI, what AqBanking does not offer, and what makes the app better on Sailfish
  OS (e.g. the bank search).
- **Data protection and security come first.** Prefer the operating system's
  infrastructure (Sailfish Secrets) and standard, well-established
  cryptography. **Never invent cryptography**; when in doubt, ask the project
  owner.
- **The FinTS product registration number is never committed or pushed.** It
  lives in `src/FinTsRegistration.h`, which `.gitignore` excludes and which the
  project owner puts in place; `src/FinTsRegistration.h.example` shows the
  format. The number is compiled into the binary. Before every commit, check
  that `git status` does not list that file.
- **Threading:** anything potentially slow - network, file or database I/O,
  parsing, decoding images, many items - runs off the UI thread, in the
  author's idiom: subclass `QThread` and override `run()`. Results go back to
  the UI thread through queued signals.
- **Record architectural decisions in this file.** Say what was decided and
  why, and keep it current. A decision that changes gets rewritten, not
  appended to.

## Harbour exceptions

None. Everything the app links dynamically is on Harbour's allowed list
(Qt5Core/Gui/Qml/Quick/Sql, libsailfishapp, libsailfishsecrets), `[X-Sailjail]`
asks for `Internet` and `Secrets`, both allowed, and the spec only `Requires`
packages from `allowed_requires.conf`.

AqBanking and its dependencies are **not** on the allowed list. They are linked
**statically**, so the binary does not depend on them at all (see below). Check
the RPM with `sfdk check` after every change to the build.

Harbour's allowed lists live in the build engine, not on the web:

```sh
sfdk engine exec cat /srv/mer/toolings/SailfishOS-4.6.0.13/usr/share/sdk-harbour-rpmvalidator/allowed_libraries.conf
# allowed_requires.conf, allowed_permissions.conf and allowed_qmlimports.conf sit beside it
```

## Architecture decisions

### FinTS library: AqBanking

Decided 2026-11-02 by the project owner, out of three candidates:

- **AqBanking 6 + Gwenhywfar** (C, GPLv2/v3 resp. LGPL) - chosen. Mature (20+
  years, the FinTS engine of GnuCash and KMyMoney), actively maintained, native
  C API, and it covers what the app will need later: every TAN method
  (photoTAN, chipTAN optical/QR/USB, decoupled app approval), transactions,
  SEPA transfers, standing orders, securities, verification of payee.
- python-fints via PyOtherSide - rejected: the FinTS layer would be Python, and
  its maintainer works on it "with limited capacity".
- fints-rs (Rust) - rejected: too young (first release 2026-04), one
  developer, photoTAN undocumented.

Also out: hbci4java and libfintx (wrong platforms), the Node "fints" package
(needs Node, no balances), PSD2/XS2A REST APIs (need a BaFin-licensed provider
with eIDAS certificates) and aggregators like finAPI (a third party would see
all users' banking data).

### Third-party libraries are built here and linked statically

AqBanking needs Gwenhywfar, which needs GnuTLS and libgcrypt, which need
Nettle, GMP and libgpg-error. None of these may be linked from the system under
Harbour's rules, and GnuTLS is not even installed on the device. So
`3rdparty/build-deps.sh` builds all seven as static, position-independent
archives, and the app links them in.

- **Versions** (pinned in the script with SHA-256 checksums, taken after
  verifying the upstream OpenPGP signatures on 2026-11-02): GMP 6.3.0,
  Nettle 3.10.2, libgpg-error 1.61, libgcrypt 1.12.4, GnuTLS 3.8.13,
  Gwenhywfar 5.14.1, AqBanking 6.9.2. Upgrading one means downloading it,
  checking its signature, and updating version, URL and checksum in the
  script. **We ship these libraries, so their security fixes are ours to
  ship too**: check GnuTLS, libgcrypt and AqBanking for security releases
  before every app release.
- **Licences:** AqBanking GPLv2 or GPLv3; Gwenhywfar, GnuTLS, libgcrypt and
  libgpg-error LGPL-2.1+; Nettle and GMP LGPLv3+/GPLv2+. All compatible with the
  app's GPLv3+ and with static linking, as the app's sources are public.
- **Build**, once per architecture, inside the build engine. `build-shell`
  needs an initialised build tree, so run it from that architecture's shadow
  build directory (see [Building](#building)):

  ```sh
  cd ~/git/build-harbour-zasterbanker-4.6.0.13-aarch64
  ~/SailfishOS/bin/sfdk -c target=SailfishOS-4.6.0.13-aarch64 build-shell ../harbour-zasterbanker/3rdparty/build-deps.sh
  # likewise armv7hl and i486
  ```

  The libraries are built against 4.6 and serve the 5.1 builds of the same
  architecture as well; `deps.pri` is keyed by architecture only.
  It downloads into `3rdparty/downloads/`, builds in `3rdparty/build/<arch>/`
  (logs per library there) and installs into `3rdparty/install/<arch>/`, all
  git-ignored. Finished libraries are skipped on the next run (stamp files in
  the install dir); delete a stamp to rebuild one. The script also writes
  `3rdparty/install/<arch>/deps.pri`: compiler and linker flags, the path of
  AqBanking's `hbci.xml` and of the translations, and of the shim's internal
  headers, which it copies from AqBanking's source tree to
  `3rdparty/install/<arch>/aqbanking-internal/`. **Every path in it is
  relative to the file itself** (`$$PWD`), so the install folder is all a
  build needs: it can be moved, cached and restored without the 800 MB build
  folder, which is what the CI does (decided 2026-11-08).
- **`3rdparty/aqbanking.pri`** is what the app and the tests take of all this:
  it includes `deps.pri`, puts the `swift` parser in front of the libraries,
  adds the shim's include paths and lists AqBanking's data files the package
  installs below `/usr/share/harbour-zasterbanker/` (paths compiled into the
  libraries). Both `harbour-zasterbanker.pro` and
  `tests/engine/engine-tests.pro` include it, so the tests read exactly the
  profiles the RPM holds.
- **Build engine quirks** the script works around, all because the build
  engine sees the home folder through a shared folder: `tar` cannot set file
  times there (the script re-applies them from the archive, otherwise autotools
  tries to regenerate itself), and autotools' dependency tracking races on
  renames (disabled; it only serves incremental builds). The shared folder
  also produces the odd transient error (once: `ld: ... libgmp.a: read:
  Invalid argument` with two architectures building at once); simply run the
  script again, it continues where it stopped. A full build of all seven
  libraries takes roughly half an hour per architecture.
- **Configuration** worth knowing: GMP without assembly (its CPU detection
  would see the build host under emulation); GnuTLS without p11-kit, IDN,
  compression, TPM and tools, trusting `/etc/pki/tls/certs/ca-bundle.crt`, the
  device's CA bundle; Gwenhywfar without GUIs, without any loadable plugins,
  and without its tools except the code generators AqBanking's build needs;
  AqBanking with the `aqhbci` (FinTS) backend and `aqnone`, the
  placeholder backend it cannot start without. Plugins and data are looked
  for under `/usr/share/harbour-zasterbanker/`.
- **Translations** (decided 2026-11-04, after the project owner found the PIN
  prompt in English): Gwenhywfar and AqBanking write the texts of PIN and TAN
  prompts and many messages themselves, so both are built with NLS (glibc's
  gettext, no extra library; GnuTLS and libgpg-error stay without). They
  translate by the system locale (`LANG`, e.g. `de_DE.utf8` on the phone),
  not by the app's own language. The build engine has no `msgfmt`, so the
  script compiles their `po/*.po` (only German exists) with `tools/po2mo.py`
  into `3rdparty/install/<arch>/share/locale/`, and the app installs them to
  `/usr/share/harbour-zasterbanker/locale/`. `po2mo.py` leaves out fuzzy
  entries and any printf-format translation whose conversions differ from the
  original's, as these strings go into `snprintf`. The emulator has no
  locales but C and POSIX, so it always shows these texts in English.
- **Patches** in `3rdparty/patches/`, applied by the script:
  - `aqbanking-6.9.2-no-disk-logs.patch`: AqBanking logs every FinTS message
    (account data, balances, transactions) and every job to plain files in its
    data folder. Both are compiled out. Nothing of it reaches the disk.
  - `aqbanking-6.9.2-depot-response.patch`: `hbci.xml` defines `HIWPD` version 6
    twice, for the balance job and for `JobGetDepot`. The decoder takes the first,
    so a version 6 answer arrives as `data/BalanceInvestment`, and `JobGetDepot`,
    only looking for `data/depotResponse`, failed with "Error processing job"
    (found 2026-11-06 on the project owner's Volksbank). It now reads both.
  - `aqbanking-6.9.2-mt535-prices.patch`: the import of depot statements got
    three things wrong. The parser writes a security's price as a group that
    `AB_Security_fromDb()` does not read, so every price was lost; a typo in the
    time format dated every price the 1st of its month; tag `98C` went to a
    variable nothing read. It also dropped umlauts in names where `char` is signed.
  - `gwenhywfar-5.14.1-require-peer-cert.patch`: Gwenhywfar's HTTPS client
    would carry on with a TLS connection the server presented no certificate
    for. It now requires one.

### The shim to AqBanking's internals

AqBanking's public API covers accounts and jobs (balances, later transactions
and transfers), but setting up a FinTS user is only offered as the "control"
commands of `aqhbci-tool4`, which print to the terminal.
`src/AqHbciShim.c` does what those commands do - adduser, getsysid,
getaccounts, listitanmodes, setitanmode, settanmediumid, deluser - line by line after them,
but with structured results. It includes aqhbci's internal headers, copied
from AqBanking's source tree by `build-deps.sh`, which works because AqBanking
is linked statically.
**When upgrading AqBanking, compare the shim with the control commands in
`src/libs/plugins/backends/aqhbci/control/`.**

### Threads

- `BankingThread` (`src/BankingThread.*`, a `QThread` with `run()`) owns
  AqBanking and works off a queue of commands one at a time: initialise, add
  a bank access, fetch balances, fetch transactions, fetch holdings, remove a bank access, reset. AqBanking is
  synchronous and not thread-safe, so nothing else touches it.
- AqBanking asks for PIN, TAN or a choice by calling Gwenhywfar's GUI layer
  in the middle of a FinTS dialog. `GwenGui` (`src/GwenGui.*`) implements that
  layer: the callback asks `BankingThread::requestInteraction()`, which emits
  `interactionRequested` to the UI thread and waits on a condition variable
  until `respond()` (or a cancel, or shutdown) wakes it.
- `Banking` (`src/Banking.*`) is the QML facade, the `banking` context
  property. QML's main file pushes the matching dialog for each interaction
  (`PinDialog`, `TanDialog`, `TanMethodDialog`, `MessageDialog`); each answers
  with `banking.respond()` and closes itself if the bank's side gave up.
- A photoTAN/QR-TAN challenge image is decoded on the banking thread and
  handed to QML through `TanImageProvider` (`image://tan/<n>`), in memory only.
- The bank search (`InstituteSearch`, `InstituteSearchWorker`) runs each query
  in a worker thread with its own read-only SQLite connection. Only one runs at
  a time; when it returns and the query has changed meanwhile, it runs again
  with the latest. Keystrokes never queue up.
- **The search field keeps the keyboard while the user types** (fixed
  2026-11-03 after the project owner found it closing after every keystroke).
  It must not sit in the result list's `header`: the header is inside the
  list's focus scope, and each new result list moved the focus to the list's
  current item. It sits above the list instead, and as a second line of
  defence the page takes the focus back for the field whenever it loses it
  while `typing` - until the user closes the keyboard with Enter or picks a
  bank. `tests/search-focus.sh` checks it in the emulator: it types
  "volksbank" key by key, with results arriving in between, and screenshots
  the field, which must show the whole word with the keyboard still up.

### Setting up a bank access

One command on the banking thread, so the PIN is entered once:
add the PIN/TAN user (FinTS 3.0, server URL from the bank list) → check the
server certificate and fetch the system ID (the handshake; the bank reports
the TAN methods it allows the user) → let the user choose if there is more
than one → ask for the name of the TAN medium if the bank requires one for
that method → fetch the account list → fetch the balances. If any step but the
last fails, the half-made user is deleted again: nothing is left behind.

- **The TAN medium name** (added 2026-11-09 for Deutsche Bank and Postbank,
  whose BestSign failed without it, and works with it since; see
  [Verification status](#verification-status)).
  Some banks want, with every TAN, the name of the device or app it comes from,
  as the user named it in the bank's security settings. AqBanking sends one only
  if it was set (`aqhbci-tool4 setTanMediumId`, `ZB_SetTanMediumId()`), and does
  not know when it is needed: it reads the bank's field by its HKTAN version 2
  name, `tanMediumIdNeeded`, while versions 4 to 7 call it `needTanMediumId`. So
  `ZB_ListTanMethods()` reads it from the bank parameters itself (0 not allowed,
  1 optional, 2 required), and setup asks only if it is required: an optional
  name would send the user looking for something the bank does without. The
  user types it, in the existing text dialog, as AqBanking cannot fetch the list
  of media (`HKTAB`); adding that job would mean implementing protocol
  ourselves. Up to 32 characters, as FinTS allows; capitals count, so the field
  does not capitalise. It cannot be changed afterwards yet, other than by
  setting the access up again.

### Overview page

- **Sections are bank accesses**, keyed by `bankAccessId` rather than the bank
  name: two accesses at the same bank stay apart. There is **no section delegate**:
  a `ListView` does not lay itself out again when a section delegate grows, so a
  context menu in one covers the first account instead of pushing it down (found
  2026-11-06 on the emulator). The first account row of a bank shows the bank's
  name instead, as the last one shows the total; `section.property` stays for
  `ListView.section`, `previousSection` and `nextSection`.
- **What concerns a bank access as a whole is in the context menu of its name**
  (project owner's decision, 2026-11-06, replacing a drawer tried the day before),
  opened by a long press on the name only. A short tap does nothing. The account
  below is a separate `ListItem`, where menus per account will go.
- **Balances can be refreshed per bank access** (added 2026-11-08), from that
  menu: the same `FetchBalances` command with a `bankAccessId`, which only
  sends the jobs of that access's accounts, so no other bank is contacted and
  no other PIN asked for. The pull-down menu and the cover still refresh all.
  Setting up a bank access fetches only its own balances, for the same reason.
- **The total per bank** (added 2026-11-05) is calculated by `AccountModel`,
  as the roles `bankTotals` and `bankTotalComplete`, so it follows every
  `setBalances()` by itself. List sections have no footer, so the last account
  row of a bank shows it (`BankTotal`). There is one line per currency, as
  amounts in different currencies cannot be added up. Banks need not send a
  balance for every account: the total then adds up what there is and says
  "not all accounts included".
- **The cover shows the banks with balances and their totals**, not single
  accounts (project owner's decision, 2026-11-05), from
  `AccountModel::syncedBanks`, with the same total as the overview. More banks
  than fit fade out above the cover action.

### Transactions

- **AqBanking's `swift` parser has to be registered by hand** (found 2026-11-05).
  Transactions arrive as MT940/MT942, and AqBanking's `swift` importer asks
  Gwenhywfar for a plugin called `swift`, which is only built as a loadable
  module: without it, every statement fails. The `.pro` links the
  `lib/aqbanking/plugins/44/dbio/swift.a` from the build (the `44` changes with
  AqBanking's plugin version), in front of `-laqbanking`, and `BankingThread::run()`
  registers `dbio_swift_factory`. The importer's profiles are installed to
  `/usr/share/harbour-zasterbanker/aqbanking/data/aqbanking/imexporters/swift/profiles`.
- **CAMT comes first, MT940 is the fallback** (found 2026-11-05 on the project
  owner's Volksbank: the fetch failed at once, with no PIN prompt). AqBanking only
  tries MT940 unless an account has `AH_BANK_FLAGS_PREFER_CAMT_DOWNLOAD`, and
  creates that job only if the bank's parameter data for the account lists
  `HKKAZ`. If not, it gives up silently, without contacting the bank. So
  `ZB_SetCamtDownload()` sets the flag before each fetch (rewriting the store
  only when it changes), and AqBanking falls back to MT940 by itself if CAMT is
  not offered (that rule has an exception, next point). The `xml` importer's `default.conf` and `camt_*.conf` profiles and
  its `camt_*.xml` schemas are installed for this. **The `camt_*.conf` files are
  not optional:** AqBanking derives the formats it can offer the bank from those
  file names, and without them drops the CAMT job as silently as the MT940 one.
- **No CAMT where the account's UPD lists only another job** (found
  2026-11-11 on a Postbank credit card: 9050, 9010, 9210 "Keine Daten zur
  übermittelten Auftragsreferenz vorhanden"). AqBanking chooses the balance job
  and the MT940 one by the jobs the account's UPD lists (a credit card's are
  `DKKKS` and `DKKKU`, or `HKKKS` and `HKKKU`), but creates the CAMT job by the
  bank's parameters (BPD) alone, so with the flag set, a credit card got a CAMT
  request. GnuCash and KMyMoney users do not tick CAMT for a credit card. So
  `ZB_PreferCamtFor()` sets the flag unless the UPD lists another job for
  transactions but not `HKCAZ`: a UPD that lists `HKCAZ`, or no such job at all
  (Atruvia lists no `HKKAZ`), still means CAMT. A bank whose UPD lists `HKKAZ`
  but not `HKCAZ` gets MT940 directly, as in GnuCash and KMyMoney by default.
- **MT940 again when CAMT brings nothing** (decided 2026-11-11, for comdirect;
  see [Verification status](#verification-status)). If the CAMT request is
  accepted without a single transaction and the account's UPD lists `HKKAZ`,
  the fetch clears the flag and asks again; the answer replaces the first only
  if it holds transactions, otherwise the result stays "no transactions".
  `ZB_TransactionJobs()` tells beforehand which of the two jobs the bank's
  parameters offer, by creating them as AqBanking would, without sending
  anything, and whether the UPD lists `HKCAZ`. The flag stays cleared until the
  next fetch sets it again, so every fetch decides afresh. This is the app's answer to what GnuCash and
  KMyMoney leave to the user: their AqBanking asks for MT940 unless the user
  ticks "Prefer Statement Download as CAMT" per account, which they do where
  the bank no longer offers MT940 (Atruvia since November 2025). The
  alternative, patching AqBanking's CAMT request (it always adds the national
  account number, see comdirect below), was turned down: **AqBanking is patched
  only as a last resort**, after looking at how GnuCash and KMyMoney solve the
  problem. An account that really has no transactions costs a second request.
  The bank's messages of both requests go to the page, once each.
- **The account needs a BIC, which the account list does not bring** (found
  2026-11-06: the project owner's Volksbank answered `9160`, a mandatory field
  missing). `HKCAZ` identifies the account by IBAN and BIC, and only
  `AH_Provider_GetAccountSepaInfo()` (`HKSPA`, `aqhbci-tool4 getaccsepa`) fills in
  the BIC. `ZB_EnsureAccountBic()` does this before a fetch if the BIC is missing.
- **A bank's error without transactions is a failure**: a rejected request neither
  fails `AB_Banking_SendCommands()` nor the job, and it would otherwise read as
  "no transactions". The CAMT job also creates the account's context entry before
  looking at the response, so that entry alone proves nothing.
- **90 days back.** Under PSD2, banks generally need no TAN up to that age, and
  most ask for one or refuse anything older.
- **Fetched once per account while the app runs** (`TransactionModel` keeps them,
  in memory), so opening an account again costs no request and no TAN. Refresh
  is in the page's pull-down menu.
- **Success is not what `AB_Banking_SendCommands()` returns**: that is 0 even if
  the connection failed. No transactions are fine as well, so the check is that
  the job did not end in error, the account has an entry in the context, and there
  are transactions or no error was reported.
- Pending transactions (MT942) arrive without a currency, so the page falls back
  to the account's.
- A statement line the parser found no amount in stays in the list and shows
  "-": CAMT 052.001.08 reads the amount from `TxDtls`, and leaving such a line out
  would make the statement look complete when it is not.
- **A transaction can be copied to the clipboard** (added 2026-11-08), from its
  context menu: plain text, one line each for date (or "pending"), name, amount,
  purpose, IBAN and kind, empty ones left out. Copying is the user's choice;
  other apps can read the clipboard.
- When a job fails without an error message, the error says which way: never
  sent ("the bank does not offer...") or answer not readable. AqBanking itself
  only tells its log, which is off.
- **The bank's messages can be read on the page** (added 2026-11-09 for the
  comdirect report, as a diagnosis first). During a fetch, `BankingThread`
  keeps every result line AqBanking passes to the GUI as `HBCI: <code> -
  <text> (S|M)`, its own untranslated format, whatever the level: these are the
  only trace of a bank's warnings (3xxx), which fail neither the job nor the
  call. They go to the page before the result (`bankMessagesReported`,
  `Banking::bankMessages`). If there are any, the pull-down menu has "Messages
  from the bank" and the placeholders say so; `BankMessagesPage` lists them as
  plain text and can copy them, to paste into a problem report. Dialog opening
  and closing bring their own lines (0020, 0100, 3920...), so the list is longer
  than the one that matters; whether to filter it is open. Only for
  transactions so far.

### Depots

Added 2026-11-06, after the project owner got an error opening a depot: it went to
the transactions page, which a depot has no use for.

- **An account is a depot if AqBanking says `AB_AccountType_Investment`**, which
  comes from the FinTS account types 30-39 and 60-69. `AccountModel` has the role
  `isDepot`, and the overview opens `DepotPage` for those instead of
  `TransactionsPage`.
- **`AB_Transaction_CommandGetDepot`** (`HKWPD` versions 5 and 6, statement in
  MT535) needs the `swift` parser and the `swiftmt535.conf` profile, which were
  there for transactions already, and the patch above.
- **One depot per request.** Securities land in the context as a plain list, with
  no account to tell them apart. AqBanking's balance job also fetches them for an
  account whose parameter data lists `HKWPD`, which is why it is not used here.
- **Success** is a list of securities, or the job ending as accepted without an
  error: the bank said it carried out the request or that there are no entries
  (`3010`), which is what an empty depot looks like. AqBanking does not tell a
  statement it could not parse from that, as it overwrites the job's status with
  the bank's result code, so that case would read as an empty depot.
- **The value is units times price**, calculated by us per position, and the total
  by `HoldingModel`, per currency. The bank's own values (tag `19A`) are not read,
  as `AB_SECURITY` has no place for them.
- **Known gap: bonds and anything else not counted in units with a price in a
  currency.** The parser reads neither nominal values (`93B::AGGR//FAMT`) nor prices
  in percent (`90A::MRKT//PRCT`). Such a position shows its name and "-", and the
  total says "not all holdings included". Fixing it means carrying `19A` through a
  patch.
- Fetched once per depot while the app runs, like transactions. The value does not
  reach the overview or the totals per bank: it would make them jump when a depot
  is first opened.

### Data protection

- **Everything AqBanking stores is encrypted.** AqBanking reads and writes its
  configuration (users, accounts, the banks' parameter data, system IDs)
  through a Gwenhywfar "config manager"; the stock one writes plain text files.
  `EncryptedConfigStore` replaces it: registered as the `dir` plugin before
  AqBanking starts, it keeps everything in memory and writes it to one file,
  `~/.local/share/de.ygriega/zasterbanker/aqbanking.zbs`, sealed with
  AES-256-GCM (libgcrypt): magic and format version as authenticated data, a
  fresh random 96-bit nonce per write, a 128-bit tag. Written atomically
  (`QSaveFile`), mode 0600. It mirrors the stock manager's semantics where
  AqBanking depends on them (a group that is read before it is written
  exists, empty).
  The app keeps data of its own in the same file (`appData()`), in a group
  `zasterbanker` that `listGroups()` hides from AqBanking.
- **The key lives in Sailfish Secrets** (`KeyStore`): 256 random bits from
  libgcrypt, stored once in an app-owned collection (`ZasterBanker`, owner
  only, in the default encrypted storage) that unlocks with the device lock.
  It never touches the file system. A key that cannot be found while
  encrypted data exists is reported, never silently replaced; the way out is
  "Delete all data" on the main page, which removes data and key.
- **Sailfish OS asks the user before the app may read its key, once after every
  restart** (found 2026-11-11, after a tester got stuck at "Banking nicht
  verfügbar ... Password Agent was unable to verify the authenticity of the
  user"). The secrets daemon locks a device-lock collection whenever it
  restarts, i.e. at every reboot, and then reading from it, or deleting it,
  needs the user's confirmation through polkit (`auth_self`): the system dialog
  "harbour-zasterbanker wants to retrieve secret DataKey from collection
  ZasterBanker", with the security code if the device has one (only Confirm and
  Cancel on the emulator, which has none). Afterwards the collection stays
  unlocked until the next restart. Anything but a confirmation fails the
  request: Cancel ("The user canceled..."), no answer within 2 minutes ("Did not
  receive a reply..."), and, from the daemon's code, a refusal by polkit, which
  is the tester's message (presumably a mistyped code or failed fingerprint; not
  reproducible without a security code). So that is never a dead end: in that
  state (`Failed`), the main page's pull-down menu has **"Try again"**, which
  asks again and keeps the data, and **"Delete all data"**, for a clean start.
  `KeyStore` explains the request in its message when it was not confirmed.
  Deleting all data removes the data file first, then the collection, which
  brings the same dialog; if that is not confirmed, the data is gone, the
  message says so, and "Try again" reuses the old key for the empty store.
- **The PIN is never stored, but kept in memory for 5 minutes** (project
  owner's decision, 2026-11-07: entering it for every action was too
  annoying). `GwenGui` remembers it per bank access, by AqBanking's token
  name (`PIN_<type>_<user>`), only in the app's memory, never in Sailfish
  Secrets or on disk: that would make it outlive the app. Its rules:
  - It outlives the command it was entered in only if AqBanking reported the
    bank accepted it (`GWEN_Gui_PasswordStatus_Ok`). An unconfirmed PIN may
    be mistyped, and reusing it unasked would use up the bank's few attempts
    before it locks the access.
  - It is wiped 5 minutes after the end of the last command that used it;
    each command for that bank access starts the time again, others do not.
    Time is `CLOCK_BOOTTIME`, so time asleep counts. The banking thread wakes
    at least every 15 seconds while a PIN is kept, as its wait does not
    count suspended time, so an expired PIN is gone seconds after the device
    wakes; it is never used once expired either way.
  - It is wiped at once when the bank rejects it, when a bank access is
    removed or all data is reset (all PINs), and when the app ends.
  - The PIN dialog says so.
- **The last balances are stored, in the sealed store** (project owner's
  decision, 2026-11-07: the overview should show the last known balances at
  start). `BankingThread` keeps the last balance per account as app data
  `balances` (`QDataStream`, own format version) and publishes it with the
  accounts when AqBanking has started. Each fetch replaces the balances of the
  accounts it brought; those of removed accounts are dropped, and "Delete all
  data" removes them with the file. Not in Sailfish Secrets, although it was
  considered: the store's key is there already, so the protection is the same,
  without a second store, more requests to the Secrets daemon on every refresh,
  or a second place to clean up. Every balance shows its date, the bank's or,
  if the bank gave none, the day it was fetched, so a stored one does not pass
  for current.
- **Transactions and holdings are kept in memory only**; they are fetched on
  request. AqBanking dumps raw statements to `/tmp` if `AQHBCI_LOGBOOKED` or
  `AQHBCI_LOGNOTED` is set, so `main.cpp` unsets both.
- **TLS:** Gwenhywfar verifies the chain against the system CA bundle and the
  host name; `GwenGui` accepts a certificate only if that found nothing at
  all - there is no "accept anyway". The certificate must be present (patch
  above).
- **Logs:** Gwenhywfar and AqBanking log at error level only, and in release
  builds not at all; FinTS message and job logs are compiled out (patch above).
- **The bank's texts are not trusted** (decided 2026-11-05, after the project
  owner found literal `<br>` tags in a TAN challenge). AqBanking passes the
  challenge on as the bank sent it, formatting included, into the TAN dialog
  and, for decoupled approval, the message dialog. Both show it as
  `Text.StyledText`, not `RichText`, and `styledText()` in `GwenGui` filters it
  first: StyledText also knows `<img>`, which would load from any address, and
  `<a>`. Only `b`, `br`, `i`, `li`, `ol`, `p`, `strong`, `u` and `ul` get
  through, without attributes; other tags are dropped and the rest is escaped.
- **Transaction texts are plain text, always.** Whoever sends money writes the
  name and purpose, and Silica's `Label` defaults to `Text.AutoText`: an `<img>`
  in a purpose would make the app load from that address. Every label in
  `TransactionsPage` showing such text, and in `DepotPage` the bank's names,
  sets `Text.PlainText`; keep it that way.
  The same goes for the message in `PinDialog` (PIN and any text the bank
  asks for), which `GwenGui` hands over as plain text already.
  Known gap: the overview's account name and owner, and page headers and
  placeholders showing the bank's messages, still use the default.
- Gwenhywfar switches libgcrypt to "quick random", which downgrades requests
  for `GCRY_VERY_STRONG_RANDOM` to `GCRY_STRONG_RANDOM`. That level is still a
  cryptographically secure generator meant for keys; accepted.
- Known limit: text typed into QML fields (PIN, TAN) lives in Qt's string
  memory until it is reused; it cannot be wiped from C++.
- Known gap (found 2026-11-08 by the tests): `AB_Banking_new()` looks for an
  old AqBanking configuration to copy over, at `~/.aqbanking/settings6/users`
  and `~/.aqbanking/settings/users`, and Gwenhywfar's path lookup creates the
  folders on the way (empty). Under Sailjail the app may neither create nor
  read anything there, so this should come to nothing on the phone; without
  Sailjail, it would create the two empty folders, and copy over the plain
  configuration of another AqBanking application if there is one. Not
  patched (yet).

### Bank list

The Deutsche Kreditwirtschaft's FinTS institute list, as the project owner
receives it as registered developer (CSV export, kept outside the repository,
currently `~/Dokumente/Entwicklung/Zaster Banker/fints_institute NEU/`).
`tools/import-banklist.py` turns it into `db/fints_institutes.db`, keeping only
institutes with a PIN/TAN URL and FinTS 3.0, and adding a search key (lower
case, umlauts spelled out). Cells the spreadsheet left as `#NV` (Excel's
#N/A; a few BICs and cities) become empty. The database is **not in this
repository** - the list is licensed to registered developers - but is packaged
into the RPM. For the CI it lives in the **private** repository
[Wunderfitz/fints-institutes](https://github.com/Wunderfitz/fints-institutes)
(cloned at `~/git/fints-institutes`, decided 2026-11-08), which the build job
checks out with a read-only token. Regenerate it whenever a new list arrives,
and commit and push it there:

```sh
tools/import-banklist.py "$HOME/Dokumente/Entwicklung/Zaster Banker/fints_institute NEU/fints_institute NEU mit BIC Master.csv"
cp db/fints_institutes.db ~/git/fints-institutes/
```

That repository must stay private. `tests/data/institutes.csv` is a made-up list
in the same layout, for the tests and nothing else.

### Layout

One qmake project at the root, as in Fernschreiber and Chronograph: `src/`,
`qml/` (`pages/`, `components/`, `cover/`, `js/`), `images/`, `icons/`,
`translations/`, `rpm/`, plus `3rdparty/`, `tools/`, `tests/` and
`.github/workflows/`. Sailjail organisation
`de.ygriega`, application `zasterbanker` - `main.cpp` sets the same names, so
`QStandardPaths` points where Sailjail allows writing.

## Building

1. Place `src/FinTsRegistration.h` (see the `.example` next to it).
2. Generate `db/fints_institutes.db` (see [Bank list](#bank-list)).
3. Build the third-party libraries for each architecture (see above).
4. Build the app **out of tree**, one shadow directory per target, as
   `~/git/build-harbour-zasterbanker-<version>-<arch>`. The build engine only
   sees the home folder, so never under `/tmp`; and in-tree builds leave
   objects behind that the next architecture's link then trips over.

   ```sh
   cd ~/git/build-harbour-zasterbanker-4.6.0.13-aarch64
   ~/SailfishOS/bin/sfdk -c target=SailfishOS-4.6.0.13-aarch64 build ../harbour-zasterbanker
   ~/SailfishOS/bin/sfdk -c target=SailfishOS-4.6.0.13-aarch64 check RPMS/*.rpm
   ```

- **Both target generations are built.** The 5.1 validator rejects binaries
  built against 4.6 (`Binary does not link to __libc_start_main@GLIBC_2.34`),
  so Harbour gets 5.1 builds for aarch64 and i486, and 4.6 builds for aarch64
  and armv7hl serve devices still on 4.6. All of these passed `sfdk check`
  with its own tooling on 2026-11-03 (only warnings: unstripped binary, and
  `gethostbyname`, which GnuTLS/Gwenhywfar link).
- **Emulator**: `sfdk emulator start` runs 5.1 (i486). Deploy the 5.1 i486
  RPM **as root over SSH**, never with `sfdk deploy --sdk`: that one pops a
  confirmation dialog in the emulator and waits for a tap (it stalled a
  session once). Same version again is fine thanks to `--force`:

  ```sh
  K=~/SailfishOS/vmshare/ssh/private_keys/sdk
  R=~/git/build-harbour-zasterbanker-5.1.0.11-i486/RPMS/harbour-zasterbanker-0.5.0-1.i486.rpm
  scp -i $K -P 2223 $R root@127.0.0.1:/tmp/
  ssh -i $K -p 2223 root@127.0.0.1 "rpm -U --force /tmp/$(basename $R) && rm /tmp/$(basename $R)"
  ```

  Launch it from SSH as the launcher would:
  `sailjail -p harbour-zasterbanker.desktop /usr/bin/harbour-zasterbanker`.
  A screenshot is `VBoxManage controlvm SailfishOS-5.1.0.11 screenshotpng <file>`,
  typing into the focused field `VBoxManage controlvm SailfishOS-5.1.0.11
  keyboardputstring <text>`. A real tap or long press goes through VirtualBox's
  Python API (`vboxapi`): `console.mouse.putMouseEventAbsolute(x, y, 0, 0, 1)`,
  then the same with `0` to let go, in screenshot pixels; writing to
  `/dev/input/event*` in the guest does not reach the compositor. The SDK key also
  logs in as `root` there, which the tests use to patch the installed QML
  temporarily.

### Tests

Decided 2026-11-08, together with the CI: what had been checked by hand in the
emulator so far (see [Verification status](#verification-status)) is now
checked on every push.

- **`tests/host/`**: the tools in `tools/`, run as programs on made-up input,
  with nothing but Python 3: `import-banklist.py` (what is kept, merged, left
  empty, the search key) and `po2mo.py` (fuzzy, untranslated and obsolete
  entries, and printf formats that do not match, all left out), checked by
  reading the output with Python's own gettext.

  ```sh
  python3 -m unittest discover -s tests/host -v
  ```

- **`tests/engine/`**: one program, `tst_engine`, linking the real sources with
  AqBanking, Gwenhywfar and Qt 5.6 as the app has them, and printing PASS or
  FAIL per check as the author's other apps do. It covers
  `EncryptedConfigStore` (the file format, decrypted independently, wrong key
  and every kind of damage refused, fresh nonces, mode 0600, the "dir"
  manager's behaviour AqBanking relies on), `GwenGui` through Gwenhywfar's own
  GUI API with a stand-in banking thread (`FakeBankingThread.cpp`: every rule of
  the PIN memory, with a fake clock through `GwenGui::setClock()`, the filter on
  the bank's texts, TAN images, message boxes, the certificate check),
  statements read the way the app reads them (AqBanking started on the
  encrypted store, its real importers with the profiles the package installs,
  merged as the aqhbci jobs merge them, then `StatementReader`: MT940, MT942,
  CAMT 052.001.08 and .02, MT535 with all three fixes of the MT535 patch, the
  choice of balance), the TAN methods the shim lists from made-up bank
  parameters (whether a TAN medium is required, in both of HKTAN's field
  names) and the TAN medium it stores, the jobs for transactions it finds for
  made-up accounts (CAMT, MT940 or a credit card's job only where the UPD lists
  it, and whether it lists `HKCAZ`), the choice of CAMT from those, and the
  CAMT flag it sets and clears, the models, and the bank search on the
  made-up list.
  `StatementReader` was taken out of `BankingThread.cpp` for this, unchanged.
  `tests/run-engine-tests.sh` builds it in the build engine, from the i486
  build directory, whose binaries run natively; it installs AqBanking's data
  files with `make install` and makes the bank list from
  `tests/data/institutes.csv`:

  ```sh
  cd ~/git/build-harbour-zasterbanker-4.6.0.13-i486
  ~/SailfishOS/bin/sfdk -c target=SailfishOS-4.6.0.13-i486 build-shell ../harbour-zasterbanker/tests/run-engine-tests.sh
  ```

  Two things the build engine needs: the program links with `--as-needed`, as
  the package build does (GnuTLS's flags name libatomic, which the target has
  no runtime library of), and it replaces `getpwuid()`, as the target has no
  passwd entry for the build user and `AB_Banking_new()` aborts without a home
  directory. The tests' home folder is an empty one of their own, and they
  check that AqBanking writes no file there.
- Not covered: `KeyStore` (needs the Sailfish Secrets daemon), anything that
  talks to a bank (so neither the fallback to MT940 itself), which job
  AqBanking sends for the flag (done once on the emulator on 2026-11-05), and the UI
  (`tests/search-focus.sh` needs the emulator).

### Continuous integration

`.github/workflows/build.yml` (added 2026-11-08), modelled on Chronograph's:

- **Pushes to master build a nightly**, kept as the run's artifacts for 30 days;
  **a tag on master is a release**, a semantic version, with the changelog's
  entry as notes. Nothing runs for pull requests or other branches, and nothing
  by hand: those would get near the secrets. The project owner accepted that
  nightlies are effectively published builds, with the real registration number
  and the bank list inside (2026-11-08): anyone signed in to GitHub can download
  a public repository's artifacts.
- **Jobs**: version → libraries (per architecture) → tests → build of the four
  packages (4.6 aarch64 and armv7hl, 5.1 aarch64 and i486), each followed by
  Harbour's validation (`mb2 check`, as `sfdk check`) → release. The RPMs are
  renamed after their target (`harbour-zasterbanker-0.5.0-1.sfos5.1.aarch64.rpm`),
  as the 4.6 and 5.1 aarch64 packages would share a file name.
- **A nightly's Release is `0.<UTC time>`**, e.g. `0.202611082222`, so that the
  release it precedes (`-1`) installs over it as an update. Harbour's validation
  allows only digits, periods and underscores there (found 2026-11-08: it
  rejected `.git<commit>`), so the commit goes into the artifact's name. Test
  packages built by hand for testers follow the same scheme.
- **The libraries are kept in GitHub's Actions cache**, keyed by
  `build-deps.sh`, the patches, `po2mo.py` and the build engine's image; a run
  that finds them missing builds them itself (an hour or so per architecture,
  in parallel), without `bin/`, which only their own build needs. GitHub drops
  cache entries unused for a week, so a weekly scheduled run restores them,
  or builds them again, and does nothing else. Release assets were considered
  and rejected: they would show up next to the app's releases, and publishing
  the libraries' binaries would mean publishing their sources beside them.
- **Build engines**: `coderus/sailfishos-platform-sdk` 4.6.0.13 and 5.1.0.11,
  pinned by digest in the workflow, as they run privileged. Each job works in a
  copy of the sources inside the container, as Chronograph's build action does.
- **Secrets**, used by the build job only - never by the jobs that run the
  libraries' build scripts or the tests: `FINTS_REGISTRATION_NUMBER` becomes
  `src/FinTsRegistration.h` (letters and digits only, never printed), and
  `FINTS_DATABASE_ACCESS_TOKEN`, a fine-grained read-only token for
  `Wunderfitz/fints-institutes`, fetches the bank list; the checkout keeps no
  credentials and is removed before the build engine starts.

### Development device

**The Jolla Phone (2026) is this project's default development device**
(set by the project owner on 2026-11-03). It runs Sailfish OS 5.2.0.22 on
aarch64 and is registered in the SDK as device `"Jolla Phone (2026)"`
(`defaultuser@192.168.178.177`, key `~/.ssh/qtc_id7`). It gets the **5.1
aarch64** build, deployed from that build directory:

```sh
cd ~/git/build-harbour-zasterbanker-5.1.0.11-aarch64
~/SailfishOS/bin/sfdk -c target=SailfishOS-5.1.0.11-aarch64 -c device="Jolla Phone (2026)" deploy --sdk
```

This is the project owner's own phone with their real bank access on it:
deploy to it when asked, never run the app's banking operations from SSH,
and never read, copy or delete anything under
`~/.local/share/de.ygriega/zasterbanker/` there. The other devices in the
SDK's list belong to the project owner as well and are not used for this
project unless the owner says so.

## Verification status

- 2026-11-03, emulator (5.1): the app starts in Sailjail, creates the key in
  Sailfish Secrets, starts AqBanking on the encrypted store and shows the
  empty overview in German. The data folder holds nothing but the 211-byte
  sealed `aqbanking.zbs` (mode 0600, magic `ZBCS`); AqBanking wrote no plain
  file of its own.
- 2026-11-03, Jolla Phone (2026), Sailfish OS 5.2.0.22: the 5.1 aarch64
  build installs (with the registration number compiled in); bank list and
  `hbci.xml` are in place.
- **2026-11-03, real bank, development device: iteration 1 works end to
  end.** The project owner set up a Volksbank (Atruvia data centre,
  `fints2.atruvia.de`) through the bank search, entered the login and PIN,
  chose "smartTAN photo" from the TAN methods the bank offered, and the
  overview showed all accounts with their balances a few seconds later.
  Verified thereby: bank search, TLS to the bank, handshake (system ID),
  TAN method choice, account list, balances, all with the real registration
  number. Personal details of that access stay out of this file.
- **2026-11-05, real bank: the photoTAN dialog works.** Updating the account
  balances brought up the 2FA dialog for the first time. The coloured QR code
  showed perfectly, the project owner's reading device read it, and the bank
  accepted the TAN. Verified thereby: TAN challenge during a balance fetch,
  the challenge image through `TanImageProvider`, TAN entry and its answer.
- 2026-11-05, development device: the project owner tried the drawer on the
  overview, the totals per bank and the new cover with their real bank access
  and is happy with all three. The undo countdown was only seen on the
  emulator; the removal itself was not carried out.
- 2026-11-05, emulator (5.1): the app stops at "Banking nicht verfügbar", with
  "Password Agent was unable to verify the authenticity of the user" from
  Sailfish Secrets, also on an unmodified page. It got past that on 2026-11-03
  and the phone is not affected. UI checks on the emulator ran on sample data
  patched into the QML. Explained on 2026-11-11: after the emulator's restart,
  Sailfish OS asked whether the app may read its key, and nobody confirmed (see
  [Data protection](#data-protection)).
- 2026-11-11, emulator (5.1), real touch input, the app started from its icon
  and over SSH: after the secrets daemon's restart, the system dialog asks for
  the key; Cancel ends in "Banking unavailable" with the new explanation and a
  pull-down menu with "Delete all data" and "Try again". "Try again" asks
  again, and Confirm opens the overview with the stored data. "Delete all
  data" removes the data file and asks to delete the collection: cancelled,
  the message says the data is gone, and the next "Delete all data" with
  Confirm starts afresh with a new key and a new 211-byte store. The German
  texts were not seen (the emulator runs in English), nor the dialog with a
  security code.
- 2026-11-05, emulator (5.1): AqBanking's real parser with the installed
  profiles read sample MT940, MT942 and CAMT 052.001.02 and .08 statements into
  `TransactionModel` correctly; `ZB_PreferCamtDownload()` (now
  `ZB_SetCamtDownload()`) sets the flag and it
  stays. The samples were self-written, and the MT942 and CAMT pending path not
  run through the job's own merge. AqBanking's real job creation, fed made-up bank
  parameters, picks CAMT when offered and MT940 when that is all there is, and
  fails as seen on the phone if the CAMT profiles are missing. The
  page was checked on sample data: list, expanded row, no transactions, error,
  and markup in a purpose shown literally.
- **2026-11-06, real bank, development device: transactions arrive.** On the
  project owner's Volksbank, tapping an account asked for the PIN, and the
  transactions showed up with no error. It took all of: the `swift` and CAMT
  files, the CAMT preference and the BIC. Only a first look so far: amounts, texts,
  pending entries, TAN during a fetch, other kinds of account and accounts without
  transactions are still to be tested in detail. Whether the app really uses CAMT
  there, and the BIC's role, follow from the symptoms and the code; the temporary
  diagnostics' output for that run was not read in detail.
- 2026-11-06, emulator (5.1), sample data, real touch input: a long press on a
  bank's name opens its menu and pushes the accounts down, a short tap does
  nothing, a long press on an account opens no menu, and the entry starts the undo
  countdown with the right bank and access ID. The removal itself was not carried
  out.
- 2026-11-06, emulator (5.1): AqBanking's real importer with the installed profile
  read a self-written MT535 statement correctly once patched: units, price, currency
  and date (tags `98A` and `98C`). `DepotPage` was checked with real touch input on
  sample data fed into the real `HoldingModel`: positions, a total per currency, a
  bond without a value, an empty depot, an error, and markup in a name shown
  literally.
- **2026-11-06, real bank, development device: depots work.** On the project
  owner's Volksbank, opening a depot showed its holdings, and the project owner found
  them correct. Before the response patch, the same request failed with "Error
  processing job", so the bank's answer is `HIWPD` version 6 as far as the symptoms
  go. That depot holds only shares and the bank asked for no TAN, so funds, bonds and
  a TAN during a depot fetch are still untested on a real bank.
- 2026-11-07, emulator (5.1): the real `GwenGui` with a stand-in for the
  banking thread kept and forgot PINs as intended, through a real 5-minute
  wait: reuse within and across commands, an unconfirmed PIN forgotten after
  its command, a rejected one at once, accesses kept apart, the time restarted
  by use, expiry after 5 idle minutes.
- **2026-11-07, real bank, development device: the PIN is remembered.** On
  the project owner's Volksbank, actions after the first no longer asked for
  the PIN. Expiry after 5 minutes was not reported on separately.
- 2026-11-07, emulator (5.1): the real `BankingThread`, `EncryptedConfigStore`
  and `AccountModel` stored and restored balances as intended: sealed file with
  nothing readable and mode 0600, values bit for bit, newer balances replacing
  older ones, the others kept, the date fallback, AqBanking's groups untouched
  and its listing without the app's group, a wrong key refused, pruning.
- **2026-11-08, development device: stored balances show at start.** The
  project owner found the last known balances on the overview after restarting
  the app.
- **2026-11-08, development device: refreshing one bank access works.** The
  project owner refreshed a single bank access from its context menu, and it
  worked smoothly. Setting up a bank access, which now fetches only its own
  balances, was not tried again.
- 2026-11-08, emulator (5.1), sample rows patched into the QML, real touch
  input: a long press on a transaction opens "Copy to clipboard" in German and
  pushes the rows below down; the clipboard then held the expected lines for a
  booked transaction with markup in the name (kept literal), a pending one and
  one without name or amount.
- **2026-11-08, development device: copying a transaction works.** The project
  owner copied transactions of their real account to the clipboard, and it
  worked smoothly.
- **2026-11-08, real bank, development device: a second bank.** The project
  owner set up their access at the GLS Gemeinschaftsbank eG, and it worked
  smoothly. It is an Atruvia bank as well (`fints1.atruvia.de`, where the
  Volksbank is on `fints2`), so this says nothing about other data centres.
- **2026-11-09, real banks, testers: EthikBank and DKB.** The first testers
  reported the current feature set working at the EthikBank eG (Atruvia,
  `fints1.atruvia.de`) and the Deutsche Kreditbank (DKB, `fints.dkb.de`, its
  own server): **the first bank outside Atruvia.** Only the overall result was
  reported, not the TAN methods or which features were tried in detail
  (depots, for instance).
- 2026-11-09, real bank, tester: **comdirect** (`fints.comdirect.de`, its own
  server). Setup and balances work, but opening an account showed "Keine
  Umsätze - Die Bank meldet für die letzten 90 Tage keine", on an account the
  tester says has transactions. So the fetch counted as a success: the bank
  answered, AqBanking reported no error, and nothing in the answer was read
  as a transaction. Both AqBanking jobs end like this when the answer holds no
  statement, and the bank's warnings (3xxx) only reach the progress line, so
  the reason is unknown. comdirect had a known fault in its CAMT statements in
  January 2026 (other apps' workaround: MT940, `HKKAZ`, instead of CAMT,
  `HKCAZ`), and CAMT is what the app asks for first. The test build
  `0.5.0-0.202611092148` shows the bank's messages for a fetch (see
  [Transactions](#transactions)), to find out what comdirect says; the
  project owner chose that over falling back to MT940 when CAMT brings
  nothing, which is the next step if the messages point there.
- **2026-11-11, real bank, tester: comdirect's messages.** With that build, a
  fetch on the Girokonto brought, after the dialog's own lines (3060, 3050 "UPD
  nicht mehr aktuell", 0020, 3920 "Verfügbare TAN-Verfahren: mobileTAN", 3076),
  `3905 - Bitte senden Sie die Anfrage erneut. (M)` and `3010 - Kontonummer ist
  ungültig. Bitte Eingabe überprüfen. (S)`. So comdirect **rejects the account
  in the request**, but with code 3010, which FinTS reserves for "no entries"
  and which AqBanking counts as accepted (`job_virtual.c`): hence "Keine
  Umsätze". The likely cause, from AqBanking's source: the CAMT job (`HKCAZ`)
  always adds the national account number, sub-account and bank code to IBAN
  and BIC (`jobgettrans_camt.c` calls `AH_AccountJob_WriteNationalAccountInfoToArgs()`
  unconditionally), while the balance job (`HKSAL` 7), which works at
  comdirect, and the MT940 job (`HKKAZ` 7) send IBAN and BIC only, and every
  other job adds the national part only if the bank allows it in `HISPAS`.
  comdirect's Girokonto and depot are known to share one account number, which
  fits a bank that cannot resolve it. Not confirmed on the bank yet. GnuCash
  and KMyMoney users do not run into it: they get MT940 unless they ask for
  CAMT. So the app now asks again for MT940 when CAMT brings nothing (see
  [Transactions](#transactions)), in the test build `0.5.0-0.202611111919` for the
  tester; its result is awaited.
- 2026-11-11, real bank, tester: **a Postbank credit card's transactions fail**
  with `9050 - Teilweise fehlerhaft. (M)`, `9010 - Verarbeitung nicht möglich.
  (S)` and `9210 - Keine Daten zur übermittelten Auftragsreferenz vorhanden.
  (S)`, after the dialog's own lines (including 1040 and 1050, new BPD and UPD).
  From AqBanking's source, the app had it send a CAMT request for the card,
  which the card's UPD does not list; fixed by the rule in
  [Transactions](#transactions), which sends `DKKKU` (or what the UPD lists)
  instead. Whether Postbank's answer to that reads correctly is untested: the
  engine tests only check which job is chosen.
- 2026-11-09, real banks, testers: **Deutsche Bank and Postbank fail with
  BestSign** (`fints.deutsche-bank.de`, `hbci.postbank.de`). Setting up the
  access stops right after choosing BestSign, with "HBCI: 9999 - Dialog
  bereits geschlossen. (S)": the bank's answer to one segment ("S"; "M" would
  be the whole message). Not looked into on a real bank yet. The likely cause:
  these banks need the name of the TAN medium (the BestSign device, as named in
  the bank's security settings) in `HKTAN`, and AqBanking only sends one if it
  was set by hand (`aqhbci-tool4 setTanMediumId`); it never fetches the list of
  media (`HKTAB`). python-fints saw the same codes at Postbank in 2019, GnuCash
  users got BestSign working with a TAN medium ID set in 2025. The test build
  `0.5.0-0.202611092123` asks for the name at setup (see
  [Setting up a bank access](#setting-up-a-bank-access)).
- **2026-11-11, real bank, testers: BestSign works with the TAN medium name.**
  With the test build, the testers report BestSign working smoothly once the
  device name is entered at setup. Not reported: whether both Deutsche Bank and
  Postbank were tried, and which BestSign variant (920, 921...) was chosen.
- 2026-11-09, emulator (5.1), sample messages patched into the QML, real touch
  input: the empty transactions page says to pull down, the menu has "Messages
  from the bank", and its page lists them with markup kept literal. Copying them
  was not checked, nor the messages of a real fetch. The engine tests check the
  TAN methods and medium; the setup dialog for the medium was not seen.
- Not verified yet: Sparkassen and other data centres or private banks besides
  the DKB, the other TAN methods (as far as is known), removing a bank access,
  and the reset after an unreadable store (`DataUnreadable`; the reset after a
  key that could not be read was checked on the emulator).

## Roadmap

**Release** (project owner's decision, 2026-11-08): the app is in an early
testing phase for brave users, with a testing and development group on Telegram
(linked in the README), where the RPMs for testing are shared. It will be published in the Jolla Store, on OpenRepos and
as a GitHub release once it has proven to work with banks across several
backends; so far Atruvia banks and the DKB have been tried (2026-11-09).

Iterations are decided one at a time by the project owner. Known so far:

- All TAN methods FinTS supports: chipTAN optical (flicker code), chipTAN QR,
  decoupled (app approval; AqBanking asks through a message box, which the
  app already shows), chipTAN USB.
- Transfers and more - and beyond banking: personal finances.
