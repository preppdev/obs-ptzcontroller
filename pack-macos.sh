#!/usr/bin/env bash
#
# Build, bundle, ad-hoc codesign, and install ptz-controller into this Mac's OBS
# (~/Library/Application Support/obs-studio/plugins). For a distributable signed
# + notarized package use package-macos.sh.
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
PLUGINS_DIR="${PLUGINS_DIR:-$HOME/Library/Application Support/obs-studio/plugins}"

GEN="$HERE/.gen"
OUT="$HERE/build-macos"
mkdir -p "$GEN" "$OUT"

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

NDI_INC="${NDI_INC:-/Library/NDI SDK for Apple/include}"
QTFW=(QtCore QtGui QtWidgets QtNetwork)
cflags=(-std=c++20 -fPIC -Wall -arch "$ARCH" -mmacosx-version-min=12.0
  -I"$GEN" -I"$OBS_SRC/libobs" -I"$OBS_SRC/frontend/api" -I"$HERE/src" -I/opt/homebrew/include
  -I"$NDI_INC" -F"$QT/lib")
for fw in "${QTFW[@]}"; do cflags+=("-I$QT/lib/$fw.framework/Headers"); done

MOC_HDRS=(ptz-device visca-ip ptz-probe ptz-manager ndi-device dock)
SRCS=(plugin-main visca-ip ptz-probe ptz-manager ndi-runtime ndi-device dock)

echo "==> moc + compile ($ARCH)"
objs=()
for h in "${MOC_HDRS[@]}"; do
  "$MOC" "$HERE/src/$h.hpp" -o "$OUT/moc_$h.cpp"
  clang++ "${cflags[@]}" -c "$OUT/moc_$h.cpp" -o "$OUT/moc_$h.o"
  objs+=("$OUT/moc_$h.o")
done
for f in "${SRCS[@]}"; do
  clang++ "${cflags[@]}" -c "$HERE/src/$f.cpp" -o "$OUT/$f.o"
  objs+=("$OUT/$f.o")
done

echo "==> link"
qtlibs=()
for fw in "${QTFW[@]}"; do qtlibs+=("$QT/lib/$fw.framework/$fw"); done
clang++ "${objs[@]}" -bundle -arch "$ARCH" -mmacosx-version-min=12.0 \
  -F"$OBSF" -framework libobs "$OBSF/obs-frontend-api.dylib" \
  "${qtlibs[@]}" -Wl,-rpath,"$OBSF" -o "$OUT/$NAME"

echo "==> assemble .plugin"
BUNDLE="$OUT/$NAME.plugin"
rm -rf "$BUNDLE"
mkdir -p "$BUNDLE/Contents/MacOS" "$BUNDLE/Contents/Resources"
cp "$OUT/$NAME" "$BUNDLE/Contents/MacOS/$NAME"
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
echo "==> codesign ($CODESIGN_IDENT)"
codesign --force --deep --options runtime --sign "$CODESIGN_IDENT" "$BUNDLE/Contents/MacOS/$NAME"
codesign --force --deep --options runtime --sign "$CODESIGN_IDENT" "$BUNDLE"

echo "==> install to $PLUGINS_DIR"
rm -rf "$PLUGINS_DIR/$NAME.plugin"
cp -R "$BUNDLE" "$PLUGINS_DIR/$NAME.plugin"
echo "==> done"
otool -L "$BUNDLE/Contents/MacOS/$NAME" | grep -E "Qt|libobs|frontend" | sed 's/^/    /'
