# Zaster Banker

Personal finances for Sailfish OS. Zaster Banker talks directly to German banks
via FinTS (formerly HBCI) with PIN/TAN, shows your accounts and their balances,
and keeps everything it stores encrypted on your phone.

This is the restart of the original [Zaster Banker](https://github.com/Wunderfitz/harbour-zaster),
continuing its version numbers from 0.5 on. It is a new app, not an update of
the old one.

## Author

Sebastian J. Wolf [sebastian@ygriega.de](mailto:sebastian@ygriega.de), together with Claude

## License

Licensed under GNU GPLv3 or later.

FinTS is spoken by [AqBanking](https://www.aquamaniac.de/rdm/projects/aqbanking)
and Gwenhywfar, which are linked statically together with GnuTLS, Nettle, GMP,
libgcrypt and libgpg-error; see `3rdparty/build-deps.sh` for versions and
sources.

## Build

You need the Sailfish SDK. In short:

1. Copy `src/FinTsRegistration.h.example` to `src/FinTsRegistration.h` and
   enter your FinTS product registration number. You get one by registering
   your application at the
   [Deutsche Kreditwirtschaft](https://www.hbci-zka.de/register/prod_register.htm).
2. Generate `db/fints_institutes.db` from the FinTS institute list with
   `tools/import-banklist.py`.
3. In a build directory next to the checkout, build the third-party libraries
   in the build engine, once per architecture:
   `sfdk -c target=SailfishOS-4.6.0.13-aarch64 build-shell ../harbour-zasterbanker/3rdparty/build-deps.sh`
4. Build the app there: `sfdk -c target=SailfishOS-4.6.0.13-aarch64 build ../harbour-zasterbanker`

`CLAUDE.md` has the details and the reasoning behind them.
