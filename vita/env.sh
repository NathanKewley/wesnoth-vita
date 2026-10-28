# Source for working on the Vita port by hand: VitaSDK and meson on PATH, Vita pkg-config.
export VITASDK=/opt/vitasdk
export VITA_PORT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export PATH="$VITASDK/bin:$VITA_PORT/.venv/bin:$PATH"
export PKG_CONFIG="$VITA_PORT/deps/pkg-config"
