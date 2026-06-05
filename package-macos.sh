#!/usr/bin/env bash
#
# Build a DISTRIBUTABLE macOS package for PTZ Controller:
#   - dist/ptz-controller-<ver>-macos-<arch>.pkg   (per-user installer, no admin)
#   - dist/ptz-controller-<ver>-macos-<arch>.zip   (drop-in .plugin bundle)
#
# Signing/notarization (to install on other Macs without Gatekeeper warnings):
#   CODESIGN_IDENT="Developer ID Application: DCAP Inc. (7CSVH6559W)" \
#   CODESIGN_IDENT_INSTALLER="Developer ID Installer: DCAP Inc. (7CSVH6559W)" \
#   ./package-macos.sh
#   xcrun notarytool submit dist/<pkg> --keychain-profile isocorder-notary --wait
#   xcrun stapler staple dist/<pkg>   (and the .plugin, then re-zip)
#
set -euo pipefail

HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/.." && pwd)

NAME=ptz-controller
VERSION=0.1.0
ARCH="${ARCH:-arm64}"
BUNDLE_ID=com.dcap.${NAME}

OBS_APP="${OBS_APP:-/Applications/OBS.app}"
OBSF="$OBS_APP/Contents/Frameworks"
OBS_SRC="${OBS_SRC:-$ROOT/obs-studio-src}"
QT="${QT:-/Volumes/NVME/Claude/Qt/6.8.3/macos}"
MOC="$QT/libexec/moc"
NDI_INC="${NDI_INC:-/Library/NDI SDK for Apple/include}"

GEN="$HERE/.gen"
WORK="$HERE/build-macos"
DIST="$HERE/dist"
mkdir -p "$GEN" "$WORK" "$DIST"
rm -rf "$DIST"/*

cat > "$GEN/obsconfig.h" <<'EOF'
#pragma once
#define OBS_RELEASE_CANDIDATE 0
#define OBS_BETA 0
EOF
cat > "$HERE/src/version.h" <<EOF
#pragma once
#define PROJECT_VERSION "$VERSION"
#define PROJECT_VERSION_MAJOR 0
#define PROJECT_VERSION_MINOR 1
#define PROJECT_VERSION_PATCH 0
EOF

QTFW=(QtCore QtGui QtWidgets QtNetwork)
cflags=(-std=c++20 -fPIC -Wall -arch "$ARCH" -mmacosx-version-min=12.0
  -I"$GEN" -I"$OBS_SRC/libobs" -I"$OBS_SRC/frontend/api" -I"$HERE/src" -I/opt/homebrew/include
  -I"$NDI_INC" -F"$QT/lib")
for fw in "${QTFW[@]}"; do cflags+=("-I$QT/lib/$fw.framework/Headers"); done

MOC_HDRS=(ptz-device visca-ip ptz-probe ptz-manager ndi-device hybrid-device dock)
SRCS=(plugin-main visca-ip ptz-probe ptz-manager ndi-runtime ndi-device hybrid-device dock)

echo "==> moc + compile ($ARCH)"
objs=()
for h in "${MOC_HDRS[@]}"; do
  "$MOC" "$HERE/src/$h.hpp" -o "$WORK/moc_$h.cpp"
  clang++ "${cflags[@]}" -c "$WORK/moc_$h.cpp" -o "$WORK/moc_$h.o"
  objs+=("$WORK/moc_$h.o")
done
for f in "${SRCS[@]}"; do
  clang++ "${cflags[@]}" -c "$HERE/src/$f.cpp" -o "$WORK/$f.o"
  objs+=("$WORK/$f.o")
done

echo "==> link"
qtlibs=()
for fw in "${QTFW[@]}"; do qtlibs+=("$QT/lib/$fw.framework/$fw"); done
clang++ "${objs[@]}" -bundle -arch "$ARCH" -mmacosx-version-min=12.0 \
  -F"$OBSF" -framework libobs "$OBSF/obs-frontend-api.dylib" \
  "${qtlibs[@]}" -Wl,-rpath,"$OBSF" -o "$WORK/$NAME"

echo "==> assemble .plugin"
BUNDLE="$WORK/$NAME.plugin"
rm -rf "$BUNDLE"
mkdir -p "$BUNDLE/Contents/MacOS" "$BUNDLE/Contents/Resources"
cp "$WORK/$NAME" "$BUNDLE/Contents/MacOS/$NAME"
cp -R "$HERE/data/locale" "$BUNDLE/Contents/Resources/locale"
cat > "$BUNDLE/Contents/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple Computer//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>CFBundleName</key><string>$NAME</string>
	<key>CFBundleIdentifier</key><string>$BUNDLE_ID</string>
	<key>CFBundleVersion</key><string>$VERSION</string>
	<key>CFBundleShortVersionString</key><string>$VERSION</string>
	<key>CFBundleInfoDictionaryVersion</key><string>6.0</string>
	<key>CFBundleExecutable</key><string>$NAME</string>
	<key>CFBundlePackageType</key><string>BNDL</string>
	<key>CFBundleSupportedPlatforms</key><array><string>MacOSX</string></array>
	<key>LSMinimumSystemVersion</key><string>12.0</string>
</dict>
</plist>
EOF

CODESIGN_IDENT="${CODESIGN_IDENT:--}"
TS_FLAG=(); [[ "$CODESIGN_IDENT" != "-" ]] && TS_FLAG=(--timestamp)
echo "==> codesign .plugin ($CODESIGN_IDENT)"
codesign --force --deep --options runtime "${TS_FLAG[@]}" --sign "$CODESIGN_IDENT" "$BUNDLE/Contents/MacOS/$NAME"
codesign --force --deep --options runtime "${TS_FLAG[@]}" --sign "$CODESIGN_IDENT" "$BUNDLE"

echo "==> zip"
ditto -c -k --keepParent "$BUNDLE" "$DIST/$NAME-$VERSION-macos-$ARCH.zip"

echo "==> build .pkg (per-user install)"
PKGROOT="$WORK/pkgroot"
rm -rf "$PKGROOT"; mkdir -p "$PKGROOT"
cp -R "$BUNDLE" "$PKGROOT/"
pkgbuild --root "$PKGROOT" \
  --install-location "Library/Application Support/obs-studio/plugins" \
  --identifier "$BUNDLE_ID" --version "$VERSION" "$WORK/$NAME-component.pkg"

cat > "$WORK/distribution.xml" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
    <title>PTZ Controller</title>
    <domains enable_anywhere="false" enable_currentUserHome="true" enable_localSystem="false"/>
    <options customize="never" require-scripts="false" hostArchitectures="arm64,x86_64"/>
    <choices-outline><line choice="default"/></choices-outline>
    <choice id="default"><pkg-ref id="$BUNDLE_ID"/></choice>
    <pkg-ref id="$BUNDLE_ID" version="$VERSION">$NAME-component.pkg</pkg-ref>
</installer-gui-script>
EOF

PKG="$DIST/$NAME-$VERSION-macos-$ARCH.pkg"
if [[ -n "${CODESIGN_IDENT_INSTALLER:-}" ]]; then
  productbuild --distribution "$WORK/distribution.xml" --package-path "$WORK" \
    --sign "$CODESIGN_IDENT_INSTALLER" "$PKG"
else
  productbuild --distribution "$WORK/distribution.xml" --package-path "$WORK" "$PKG"
fi
rm -f "$WORK/$NAME-component.pkg" "$WORK/distribution.xml"

echo
echo "==> artifacts:"; ls -la "$DIST"
