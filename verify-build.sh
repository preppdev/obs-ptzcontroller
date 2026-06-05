#!/usr/bin/env bash
#
# Compile + link verification for ptz-controller without a full OBS build.
# Compiles against OBS source headers + Qt 6.8.3 frameworks and links against
# the OBS.app frameworks installed on this machine. NOT the production build.
#
set -euo pipefail

HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/.." && pwd)

OBS_SRC="${OBS_SRC:-$ROOT/obs-studio-src}"
QT="${QT:-/Volumes/NVME/Claude/Qt/6.8.3/macos}"
OBSF="${OBSF:-/Applications/OBS.app/Contents/Frameworks}"
MOC="$QT/libexec/moc"

GEN="$HERE/.gen"
OUT="$HERE/build-verify"
mkdir -p "$GEN" "$OUT"

cat > "$GEN/obsconfig.h" <<'EOF'
#pragma once
#define OBS_RELEASE_CANDIDATE 0
#define OBS_BETA 0
EOF
cat > "$HERE/src/version.h" <<'EOF'
#pragma once
#define PROJECT_VERSION "0.1.0"
#define PROJECT_VERSION_MAJOR 0
#define PROJECT_VERSION_MINOR 1
#define PROJECT_VERSION_PATCH 0
EOF

NDI_INC="${NDI_INC:-/Library/NDI SDK for Apple/include}"
QTFW=(QtCore QtGui QtWidgets QtNetwork)
flags=(-std=c++20 -fPIC -Wall
  -I"$GEN" -I"$OBS_SRC/libobs" -I"$OBS_SRC/frontend/api" -I"$HERE/src" -I/opt/homebrew/include
  -I"$NDI_INC" -F"$QT/lib")
for fw in "${QTFW[@]}"; do flags+=("-I$QT/lib/$fw.framework/Headers"); done

# Headers with Q_OBJECT need moc.
MOC_HDRS=(ptz-device visca-ip ptz-probe ptz-manager ndi-device dock)
SRCS=(plugin-main visca-ip ptz-probe ptz-manager ndi-runtime ndi-device dock)

echo "==> moc"
mocs=()
for h in "${MOC_HDRS[@]}"; do
  "$MOC" "$HERE/src/$h.hpp" -o "$OUT/moc_$h.cpp"
  mocs+=("moc_$h")
done

echo "==> compile"
objs=()
for f in "${SRCS[@]}"; do
  clang++ "${flags[@]}" -c "$HERE/src/$f.cpp" -o "$OUT/$f.o"
  objs+=("$OUT/$f.o")
done
for m in "${mocs[@]}"; do
  clang++ "${flags[@]}" -c "$OUT/$m.cpp" -o "$OUT/$m.o"
  objs+=("$OUT/$m.o")
done

echo "==> link (bundle)"
qtlibs=()
for fw in "${QTFW[@]}"; do qtlibs+=("$QT/lib/$fw.framework/$fw"); done
clang++ "${objs[@]}" -bundle \
  -F"$OBSF" -framework libobs "$OBSF/obs-frontend-api.dylib" \
  "${qtlibs[@]}" \
  -o "$OUT/ptz-controller.so"

echo "==> OK: $OUT/ptz-controller.so"
nm -gU "$OUT/ptz-controller.so" | grep -E "_obs_module_(load|unload|ver)$" || true
