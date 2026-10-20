#!/bin/bash
# Builds the macOS installer: Closer-Installer.pkg
# Run from the repository root after the CMake build (GitHub Actions does it).
#
# The installer copies the plugins to the standard system folders:
#   /Library/Audio/Plug-Ins/VST3/Closer.vst3
#   /Library/Audio/Plug-Ins/Components/Closer.component

set -euo pipefail

ARTEFACTS="build/Closer_artefacts/Release"
RESOURCES="packaging/macos"
WORK="pkgwork"
ID="com.ravefactory.closer"
VERSION=$(sed -nE 's/^project \(Closer VERSION ([0-9.]+)\).*/\1/p' CMakeLists.txt)

echo "Building Closer-Installer.pkg, version $VERSION"

rm -rf "$WORK"
mkdir -p "$WORK/vst3-root" "$WORK/au-root" "$WORK/packages"

cp -R "$ARTEFACTS/VST3/Closer.vst3" "$WORK/vst3-root/"
cp -R "$ARTEFACTS/AU/Closer.component" "$WORK/au-root/"

# One small package per format
make_package () {
    local root="$1" location="$2" id="$3" output="$4"
    local plist="$WORK/$id.plist"

    pkgbuild --analyze --root "$root" "$plist"

    # Always install in the standard folder, even if another copy of the
    # plugin exists somewhere else on the disk (otherwise macOS may "update"
    # that other copy instead).
    local i=0
    while /usr/libexec/PlistBuddy -c "Set :$i:BundleIsRelocatable false" "$plist" 2>/dev/null; do
        i=$((i + 1))
    done

    pkgbuild --root "$root" \
             --component-plist "$plist" \
             --identifier "$id" \
             --version "$VERSION" \
             --install-location "$location" \
             "$WORK/packages/$output"
}

make_package "$WORK/vst3-root" "/Library/Audio/Plug-Ins/VST3"       "$ID.vst3" "Closer-VST3.pkg"
make_package "$WORK/au-root"   "/Library/Audio/Plug-Ins/Components" "$ID.au"   "Closer-AU.pkg"

# Join both into one installer with a welcome screen and a choice of formats.
# If a "Developer ID Installer" identity is given (GitHub Actions does it when
# the signing secrets exist), the installer is signed with it.
SIGN_ARGS=()
if [ -n "${INSTALLER_IDENTITY:-}" ]; then
    echo "Signing the installer with: $INSTALLER_IDENTITY"
    SIGN_ARGS=(--sign "$INSTALLER_IDENTITY")
    if [ -n "${SIGNING_KEYCHAIN:-}" ]; then
        SIGN_ARGS+=(--keychain "$SIGNING_KEYCHAIN")
    fi
fi

productbuild --distribution "$RESOURCES/distribution.xml" \
             --resources "$RESOURCES/resources" \
             --package-path "$WORK/packages" \
             --version "$VERSION" \
             "${SIGN_ARGS[@]+"${SIGN_ARGS[@]}"}" \
             Closer-Installer.pkg

echo "Done: Closer-Installer.pkg"
