#!/usr/bin/env bash
# Install the ISPC compiler into ~/.local/opt (no root needed).
# Behind a proxy, export https_proxy first, e.g.
#   https_proxy=http://127.0.0.1:7897 ./scripts/install_ispc.sh
set -euo pipefail

VERSION="${ISPC_VERSION:-1.28.1}"
PREFIX="${HOME}/.local/opt"
NAME="ispc-v${VERSION}-linux"
URL="https://github.com/ispc/ispc/releases/download/v${VERSION}/${NAME}.tar.gz"

if [ -x "${PREFIX}/${NAME}/bin/ispc" ]; then
    echo "ISPC already installed: ${PREFIX}/${NAME}/bin/ispc"
    "${PREFIX}/${NAME}/bin/ispc" --version
    exit 0
fi

mkdir -p "${PREFIX}"
TMP="$(mktemp -d)"
trap 'rm -rf "${TMP}"' EXIT

echo "Downloading ${URL}"
curl -fSL -o "${TMP}/${NAME}.tar.gz" "${URL}"
tar -xzf "${TMP}/${NAME}.tar.gz" -C "${PREFIX}"

"${PREFIX}/${NAME}/bin/ispc" --version
echo
echo "Installed. The Makefiles find it automatically. To use it from the shell:"
echo "  export PATH=\$PATH:${PREFIX}/${NAME}/bin"
