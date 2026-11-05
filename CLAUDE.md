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
  `3rdparty/install/<arch>/deps.pri`, which the `.pro` includes: compiler and
  linker flags, the path of AqBanking's `hbci.xml`, and AqBanking's source
  tree for the shim's internal headers.
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
  - `gwenhywfar-5.14.1-require-peer-cert.patch`: Gwenhywfar's HTTPS client
    would carry on with a TLS connection the server presented no certificate
    for. It now requires one.

### The shim to AqBanking's internals

AqBanking's public API covers accounts and jobs (balances, later transactions
and transfers), but setting up a FinTS user is only offered as the "control"
commands of `aqhbci-tool4`, which print to the terminal.
`src/AqHbciShim.c` does what those commands do - adduser, getsysid,
getaccounts, listitanmodes, setitanmode, deluser - line by line after them,
but with structured results. It includes aqhbci's internal headers from the
unpacked source tree, which works because AqBanking is linked statically.
**When upgrading AqBanking, compare the shim with the control commands in
`src/libs/plugins/backends/aqhbci/control/`.**

### Threads

- `BankingThread` (`src/BankingThread.*`, a `QThread` with `run()`) owns
  AqBanking and works off a queue of commands one at a time: initialise, add
  a bank access, fetch balances, remove a bank access, reset. AqBanking is
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
than one → fetch the account list → fetch the balances. If any step but the
last fails, the half-made user is deleted again: nothing is left behind.

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
- **The key lives in Sailfish Secrets** (`KeyStore`): 256 random bits from
  libgcrypt, stored once in an app-owned collection (`ZasterBanker`, owner
  only, in the default encrypted storage) that unlocks with the device lock.
  It never touches the file system. A key that cannot be found while
  encrypted data exists is reported, never silently replaced; the way out is
  "Delete all data" on the main page, which removes data and key.
- **The PIN is never stored.** `GwenGui` remembers it for the duration of one
  command (a setup runs several FinTS dialogs) and wipes it when the command
  ends or the bank rejects it.
- **Balances are kept in memory only**; they are fetched on request.
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
- Gwenhywfar switches libgcrypt to "quick random", which downgrades requests
  for `GCRY_VERY_STRONG_RANDOM` to `GCRY_STRONG_RANDOM`. That level is still a
  cryptographically secure generator meant for keys; accepted.
- Known limit: text typed into QML fields (PIN, TAN) lives in Qt's string
  memory until it is reused; it cannot be wiped from C++.

### Bank list

The Deutsche Kreditwirtschaft's FinTS institute list, as the project owner
receives it as registered developer (CSV export, kept outside the repository,
currently `~/Dokumente/Entwicklung/Zaster Banker/fints_institute NEU/`).
`tools/import-banklist.py` turns it into `db/fints_institutes.db`, keeping only
institutes with a PIN/TAN URL and FinTS 3.0, and adding a search key (lower
case, umlauts spelled out). Cells the spreadsheet left as `#NV` (Excel's
#N/A; a few BICs and cities) become empty. The database is **not in Git** - the list is
licensed to registered developers - but is packaged into the RPM. Regenerate it
whenever a new list arrives:

```sh
tools/import-banklist.py "$HOME/Dokumente/Entwicklung/Zaster Banker/fints_institute NEU/fints_institute NEU mit BIC Master.csv"
```

### Layout

One qmake project at the root, as in Fernschreiber and Chronograph: `src/`,
`qml/` (`pages/`, `components/`, `cover/`, `js/`), `images/`, `icons/`,
`translations/`, `rpm/`, plus `3rdparty/` and `tools/`. Sailjail organisation
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
  keyboardputstring <text>`. The SDK key also logs in as `root` there, which
  the tests use to patch the installed QML temporarily.

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
- Not verified yet: banks in other data centres (Sparkassen, private banks),
  the other TAN methods, removing a bank access, and the reset after an
  unreadable store.

## Roadmap

Iterations are decided one at a time by the project owner. Known so far:

- All TAN methods FinTS supports: chipTAN optical (flicker code), chipTAN QR,
  decoupled (app approval; AqBanking asks through a message box, which the
  app already shows), chipTAN USB.
- Transactions, transfers and more - and beyond banking: personal finances.
