#!/bin/bash
#
# Builds the third-party libraries Zaster Banker links statically:
#   gmp, nettle, libgpg-error, libgcrypt, gnutls, gwenhywfar, aqbanking
#
# None of them is a Harbour-allowed system library, therefore they are built
# as static, position-independent archives and linked into the app binary.
#
# Must run inside the Sailfish SDK build engine for the target architecture:
#   ~/SailfishOS/bin/sfdk -c target=SailfishOS-4.6.0.13-aarch64 build-shell ./3rdparty/build-deps.sh
#
# Results are staged into 3rdparty/install/<arch>/ (git-ignored). The app's
# qmake project picks them up via 3rdparty/install/<arch>/deps.pri.
#
# Tarball checksums were pinned after verifying the upstream OpenPGP
# signatures (see CLAUDE.md, section "Third-party libraries").

set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ARCH="$(uname -m)"
case "$ARCH" in
    armv7*) ARCH=armv7hl ;;
    i?86)   ARCH=i486 ;;
esac

DOWNLOADS="$HERE/downloads"
BUILD="$HERE/build/$ARCH"
STAGE="$HERE/install/$ARCH"
PATCHES="$HERE/patches"

# Paths on the device (inside the app's RPM payload)
APP_DATA=/usr/share/harbour-zasterbanker
DEVICE_CA_BUNDLE=/etc/pki/tls/certs/ca-bundle.crt

JOBS="${JOBS:-$(nproc)}"

#        name            version  file                              sha256                                                            url
PKGS=(
    "gmp|6.3.0|gmp-6.3.0.tar.xz|a3c2b80201b89e68616f4ad30bc66aee4927c3ce50e33929ca819d5c43538898|https://ftp.gnu.org/gnu/gmp/gmp-6.3.0.tar.xz"
    "nettle|3.10.2|nettle-3.10.2.tar.gz|fe9ff51cb1f2abb5e65a6b8c10a92da0ab5ab6eaf26e7fc2b675c45f1fb519b5|https://ftp.gnu.org/gnu/nettle/nettle-3.10.2.tar.gz"
    "libgpg-error|1.61|libgpg-error-1.61.tar.bz2|7a85413f2bc354f4f8aa832b718af122e48965e9e0eb9012ee659c13c6385c93|https://www.gnupg.org/ftp/gcrypt/libgpg-error/libgpg-error-1.61.tar.bz2"
    "libgcrypt|1.12.4|libgcrypt-1.12.4.tar.bz2|d77f68f48879510e79a2f65977ccc68981781ea0923e5bdffac2a193ea3d660e|https://www.gnupg.org/ftp/gcrypt/libgcrypt/libgcrypt-1.12.4.tar.bz2"
    "gnutls|3.8.13|gnutls-3.8.13.tar.xz|ffed8ec1bf09c2426d4f14aae377de4753b53e537d685e604e99a8b16ca9c97e|https://www.gnupg.org/ftp/gcrypt/gnutls/v3.8/gnutls-3.8.13.tar.xz"
    "gwenhywfar|5.14.1|gwenhywfar-5.14.1.tar.gz|8916feaa99cb954f963f2cba8dd2dffe57cacf7f284daf00eab071aad6fe2ab3|https://www.aquamaniac.de/rdm/attachments/download/630/gwenhywfar-5.14.1.tar.gz"
    "aqbanking|6.9.2|aqbanking-6.9.2.tar.gz|244b5f7a139f829928f5cdc3f5f7488517b3e8aa63625a92741c3efc1892bb3f|https://www.aquamaniac.de/rdm/attachments/download/654/aqbanking-6.9.2.tar.gz"
)

export CFLAGS="-O2 -g0 -fPIC -fstack-protector-strong -D_FORTIFY_SOURCE=2"
export CXXFLAGS="$CFLAGS"
export CPPFLAGS="-I$STAGE/include"
export LDFLAGS="-L$STAGE/lib"
export PKG_CONFIG_PATH="$STAGE/lib/pkgconfig"
export PATH="$STAGE/bin:$PATH"

# Dependency tracking only serves incremental builds, and its .deps/*.Tpo
# renames race on the build engine's shared folder.
COMMON_CONF=(--prefix="$STAGE" --enable-static --disable-shared --with-pic --disable-dependency-tracking)

log() { echo "==> [$ARCH] $*"; }

fetch_and_unpack() {
    local name="$1" version="$2" file="$3" sha="$4" url="$5"
    mkdir -p "$DOWNLOADS" "$BUILD"
    if [ ! -f "$DOWNLOADS/$file" ]; then
        log "Downloading $file"
        curl -sSLf --retry 3 -o "$DOWNLOADS/$file.part" "$url"
        mv "$DOWNLOADS/$file.part" "$DOWNLOADS/$file"
    fi
    echo "$sha  $DOWNLOADS/$file" | sha256sum -c --quiet - || {
        echo "Checksum mismatch for $file" >&2
        exit 1
    }
    rm -rf "$BUILD/$name-$version"
    tar -C "$BUILD" -xf "$DOWNLOADS/$file"
    # tar cannot set mtimes on the build engine's shared home folder, which
    # leaves every file at "now" in archive order and makes autotools want to
    # regenerate itself. Apply the archive's mtimes by path instead.
    python3 - "$DOWNLOADS/$file" "$BUILD" <<'PYEOF'
import os, sys, tarfile
with tarfile.open(sys.argv[1]) as archive:
    members = archive.getmembers()
for member in reversed(members):
    path = os.path.join(sys.argv[2], member.name)
    if os.path.lexists(path) and not member.issym():
        os.utime(path, (member.mtime, member.mtime))
PYEOF
    local p
    for p in "$PATCHES/$name-$version"-*.patch; do
        [ -e "$p" ] || continue
        log "Applying $(basename "$p")"
        patch -d "$BUILD/$name-$version" -p1 < "$p"
    done
}

build_gmp() {
    # Generic C code only: under the build engine's CPU emulation, GMP's
    # CPU detection would otherwise pick the host CPU.
    ./configure "${COMMON_CONF[@]}" --disable-assembly --disable-cxx
}

build_nettle() {
    local extra=()
    [ "$ARCH" = "i486" ] && extra+=(--disable-assembler)
    [ "$ARCH" != "i486" ] && extra+=(--enable-fat)
    ./configure "${COMMON_CONF[@]}" --libdir="$STAGE/lib" \
        --disable-documentation --disable-openssl --disable-mini-gmp "${extra[@]}"
}

build_libgpg-error() {
    ./configure "${COMMON_CONF[@]}" --disable-doc --disable-tests --disable-nls \
        --disable-languages
}

build_libgcrypt() {
    local extra=()
    [ "$ARCH" = "i486" ] && extra+=(--disable-asm)
    ./configure "${COMMON_CONF[@]}" --disable-doc --with-libgpg-error-prefix="$STAGE" \
        --disable-jent-support "${extra[@]}"
}

build_gnutls() {
    ./configure "${COMMON_CONF[@]}" \
        --with-included-libtasn1 --with-included-unistring \
        --without-p11-kit --without-idn --without-zlib --without-brotli --without-zstd \
        --without-tpm --without-tpm2 --without-leancrypto \
        --disable-tools --disable-doc --disable-manpages --disable-tests --disable-cxx \
        --disable-nls --disable-guile --disable-libdane --disable-ktls \
        --with-default-trust-store-file="$DEVICE_CA_BUNDLE"
}

build_gwenhywfar() {
    ./configure "${COMMON_CONF[@]}" \
        --with-guis="" --with-plugins-ct="" --with-plugins-dbio="" --with-plugins-cfgmgr="" \
        --enable-system-certs \
        --with-plugin-searchdir="$APP_DATA/gwenhywfar/plugins" \
        --with-locale-searchdir="$APP_DATA/locale" \
        LIBGCRYPT_CONFIG="$STAGE/bin/libgcrypt-config" &&
    # Only the code generators AqBanking's build runs are needed. gwbuild does
    # not even link against a static libgwenhywfar (library order), and
    # gct-tool would pull in the system's OpenSSL.
    sed -i 's/^SUBDIRS = .*/SUBDIRS = xmlmerge typemaker typemaker2/' tools/Makefile
}

build_aqbanking() {
    ./configure "${COMMON_CONF[@]}" \
        --with-backends="aqhbci aqnone" \
        --with-plugin-searchdir="$APP_DATA/aqbanking/plugins" \
        --with-cfg-searchdir="$APP_DATA/aqbanking/etc" \
        --with-locale-searchdir="$APP_DATA/locale" \
        --with-data-searchdir="$APP_DATA/aqbanking/data" \
        --with-typemaker2-exe="$STAGE/bin/typemaker2" \
        --with-xmlmerge="$STAGE/bin/xmlmerge"
}

# Gwenhywfar and AqBanking talk to the user (PIN and TAN prompts, the bank's
# messages), so both are built with NLS, served by glibc's gettext. The build
# engine has no msgfmt, so their catalogues are compiled here; the app installs
# them where both look (--with-locale-searchdir).
compile_catalogues() {
    local entry name version file sha url po lang
    for entry in "${PKGS[@]}"; do
        IFS='|' read -r name version file sha url <<< "$entry"
        [ "$name" = gwenhywfar ] || [ "$name" = aqbanking ] || continue
        for po in "$BUILD/$name-$version"/po/*.po; do
            lang="$(basename "$po" .po)"
            mkdir -p "$STAGE/share/locale/$lang/LC_MESSAGES"
            python3 "$HERE/../tools/po2mo.py" "$po" "$STAGE/share/locale/$lang/LC_MESSAGES/$name.mo"
        done
    done
}

# The shim (src/AqHbciShim.c) includes aqhbci's internal headers, which AqBanking
# does not install. They are copied next to the libraries, in the source tree's
# layout, so that the install folder is all the app's build needs: it can be
# moved, cached and restored without the build folder.
copy_internal_headers() {
    local source="$BUILD/aqbanking-6.9.2"
    local target="$STAGE/aqbanking-internal"
    if [ ! -d "$source/src/libs" ]; then
        [ -d "$target/src/libs" ] && return 0
        echo "AqBanking's build folder is gone and its headers were never copied; delete $STAGE/.built-aqbanking-6.9.2 and run again" >&2
        exit 1
    fi
    rm -rf "$target"
    mkdir -p "$target"
    (cd "$source" && find src/libs -name '*.h' -print0 | tar --null -cf - -T -) | tar -C "$target" -xf -
}

# Every path relative to deps.pri itself ($$PWD in qmake is the folder of the
# file being read), so that the install folder works wherever it ends up.
write_pri() {
    local cflags libs
    cflags="$(pkg-config --cflags aqbanking gwenhywfar)"
    libs="$(pkg-config --static --libs aqbanking gwenhywfar gnutls libgcrypt)"
    cat > "$STAGE/deps.pri" <<EOF
# Generated by 3rdparty/build-deps.sh - do not edit
QMAKE_CFLAGS += ${cflags//$STAGE/\$\$PWD}
QMAKE_CXXFLAGS += ${cflags//$STAGE/\$\$PWD}
LIBS += ${libs//$STAGE/\$\$PWD}
ZB_AQHBCI_XML = \$\$PWD/share/aqbanking/backends/aqhbci/hbci.xml
ZB_LOCALE_DIR = \$\$PWD/share/locale
ZB_AQBANKING_SRC = \$\$PWD/aqbanking-internal
EOF
    log "Wrote $STAGE/deps.pri"
}

mkdir -p "$STAGE"
for entry in "${PKGS[@]}"; do
    IFS='|' read -r name version file sha url <<< "$entry"
    stamp="$STAGE/.built-$name-$version"
    if [ -f "$stamp" ]; then
        log "$name $version already built"
        continue
    fi
    fetch_and_unpack "$name" "$version" "$file" "$sha" "$url"
    log "Building $name $version"
    # Chained with && on purpose: set -e does not apply inside a subshell
    # that is the left-hand side of ||.
    (
        cd "$BUILD/$name-$version" &&
        "build_$name" &&
        make -j"$JOBS" &&
        make install
    ) > "$BUILD/$name.log" 2>&1 || {
        echo "Build of $name failed, see $BUILD/$name.log" >&2
        tail -40 "$BUILD/$name.log" >&2
        exit 1
    }
    touch "$stamp"
done

compile_catalogues
copy_internal_headers
write_pri
log "Done"
