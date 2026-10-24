#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd -- "${SCRIPT_DIR}/../.." && pwd)"

APP_VERSION="${APP_VERSION:-$(sed -nE 's/^[[:space:]]*set\(MSMS_VERSION "([0-9]+\.[0-9]+\.[0-9]+)".*/\1/p' "${ROOT_DIR}/CMakeLists.txt" | head -n1)}"
JOBS="${JOBS:-2}"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build/ci-linux}"
OUT_DIR="${OUT_DIR:-${ROOT_DIR}/out}"
TOOLS_DIR="${TOOLS_DIR:-${BUILD_DIR}/packaging-tools}"
WORK_DIR="${WORK_DIR:-${BUILD_DIR}/appimage-work}"
APPDIR="${WORK_DIR}/AppDir"
PACKAGE_DIR="${WORK_DIR}/output"
APP_BINARY="${BUILD_DIR}/msms"

die() {
    printf 'appimage.sh: error: %s\n' "$*" >&2
    exit 1
}

[[ "${APP_VERSION}" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] ||
    die "APP_VERSION must be MAJOR.MINOR.PATCH (for example 1.2.3); got '${APP_VERSION}'."

[[ "$(uname -s)" == "Linux" ]] || die "AppImage packaging must run on Linux."
command -v getconf >/dev/null 2>&1 || die "getconf is required to verify the glibc build floor."
GLIBC_VERSION="$(getconf GNU_LIBC_VERSION | awk '{print $2}')"
[[ "${GLIBC_VERSION}" == "2.31" ]] ||
    die "build on glibc 2.31 to preserve Debian 11 / Ubuntu 20.04 compatibility (detected ${GLIBC_VERSION}). Use the Ubuntu 20.04 release container."

for command_name in cmake ninja pkg-config curl file rsvg-convert; do
    command -v "${command_name}" >/dev/null 2>&1 ||
        die "required command '${command_name}' is missing; install the packaging build dependencies."
done
pkg-config --exists gtk+-3.0 ||
    die "GTK 3 development files are required (pkg-config gtk+-3.0 was not found)."
pkg-config --exists gobject-introspection-1.0 ||
    die "GObject Introspection development files are required by linuxdeploy-plugin-gtk."

# Force the bundled, static wxGTK build. SQLite is linked from the in-tree
# amalgamation into the static sqlite3_lib target.
cmake --preset ci-linux \
    -DMSMS_VERSION="${APP_VERSION}" \
    -DCMAKE_DISABLE_FIND_PACKAGE_wxWidgets=ON \
    -DwxBUILD_SHARED=OFF \
    -DwxBUILD_TESTS=OFF \
    -DwxBUILD_SAMPLES=OFF \
    -DwxBUILD_DEMOS=OFF \
    -DwxBUILD_INSTALL=OFF

cmake --build --preset ci-linux --parallel "${JOBS}"
ctest --preset ci-linux --output-on-failure

[[ -x "${APP_BINARY}" ]] || die "expected release executable was not produced at ${APP_BINARY}."
if ldd "${APP_BINARY}" 2>/dev/null | grep -E 'libwx[^[:space:]]*\.so|libsqlite3\.so'; then
    die "the executable links to shared wxWidgets or SQLite; check the static dependency build."
fi

mkdir -p "${TOOLS_DIR}" "${PACKAGE_DIR}" "${OUT_DIR}"
if [[ ! -x "${TOOLS_DIR}/linuxdeploy" ]]; then
    curl -fsSL \
        -o "${TOOLS_DIR}/linuxdeploy" \
        "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
    chmod +x "${TOOLS_DIR}/linuxdeploy"
fi
if [[ ! -x "${TOOLS_DIR}/linuxdeploy-plugin-gtk" ]]; then
    curl -fsSL \
        -o "${TOOLS_DIR}/linuxdeploy-plugin-gtk" \
        "https://raw.githubusercontent.com/linuxdeploy/linuxdeploy-plugin-gtk/master/linuxdeploy-plugin-gtk.sh"
    chmod +x "${TOOLS_DIR}/linuxdeploy-plugin-gtk"
fi

rm -rf "${APPDIR}" "${PACKAGE_DIR}"
mkdir -p "${APPDIR}/usr/bin" "${PACKAGE_DIR}"
cp "${APP_BINARY}" "${APPDIR}/usr/bin/msms-manager"
cp "${ROOT_DIR}/packaging/linux/msms-manager.desktop" "${APPDIR}/"
cp "${ROOT_DIR}/packaging/linux/msms-manager.svg" "${APPDIR}/"
cp "${ROOT_DIR}/LICENSE" "${APPDIR}/LICENSE.txt"
rsvg-convert -w 256 -h 256 "${APPDIR}/msms-manager.svg" > "${APPDIR}/msms-manager.png"

export PATH="${TOOLS_DIR}:${PATH}"
export APPIMAGE_EXTRACT_AND_RUN=1
export DEPLOY_GTK_VERSION=3

# The GTK plugin bundles GTK/GIO, GLib schemas, GDK Pixbuf loaders, typelibs,
# and related resources. glibc itself is intentionally not bundled.
(
    cd "${PACKAGE_DIR}"
    "${TOOLS_DIR}/linuxdeploy" \
        --appdir "${APPDIR}" \
        --plugin gtk \
        --desktop-file "${APPDIR}/msms-manager.desktop" \
        --icon-file "${APPDIR}/msms-manager.png" \
        --output appimage
)

mapfile -t GENERATED_APPIMAGES < <(find "${PACKAGE_DIR}" -maxdepth 1 -type f -name '*.AppImage' -print)
(("${#GENERATED_APPIMAGES[@]}" > 0)) || die "linuxdeploy did not produce an AppImage."

FINAL_APPIMAGE="${OUT_DIR}/MSMSManager-${APP_VERSION}-x86_64.AppImage"
cp "${GENERATED_APPIMAGES[0]}" "${FINAL_APPIMAGE}"
chmod +x "${FINAL_APPIMAGE}"
printf 'Created %s\n' "${FINAL_APPIMAGE}"
