#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Signs a Windows package for the in-app updater: <zip>.sig, an ECDSA P-256
# signature over SHA-256 (DER), as tools/release.sh does for the PS5 package.
# The key is the private half of windows/release-key.pem.
#   tools/desktop/windows-release.sh <PSBox-...-windows-x86_64.zip> <private-key.pem>
set -euo pipefail
zip=${1:?usage: windows-release.sh <zip> <private-key.pem>}
key=${2:?usage: windows-release.sh <zip> <private-key.pem>}
openssl dgst -sha256 -sign "$key" -out "$zip.sig" "$zip"
# The public key it must match (a wrong secret fails here, not in the app).
root=$(cd "$(dirname "$0")/../.." && pwd)
openssl dgst -sha256 -verify "$root/windows/release-key.pem" -signature "$zip.sig" "$zip" >/dev/null
echo "  ok  signed: $zip.sig"
