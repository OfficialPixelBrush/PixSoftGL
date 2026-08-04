#!/usr/bin/env bash
#
# run_mcb173.sh — downloads (once) and launches Minecraft Beta 1.7.3
#                 in offline mode, so you can join offline-mode servers.
#
# Usage:
#   ./run_mcb173.sh [username]
#
# Requires: curl, jq, java (a Java 8 JRE works best for this old version), unzip
#
# Everything is fetched straight from Mojang's official, public metadata
# (launchermeta.mojang.com / libraries.minecraft.net / resources.download.minecraft.net),
# the same servers the real launcher uses — no third-party mirrors.

set -euo pipefail

VERSION_ID="b1.7.3"
USERNAME="${1:-Player$RANDOM}"

BASE_DIR="$HOME/.mcb173"
BIN_DIR="$BASE_DIR/bin"
NATIVES_DIR="$BIN_DIR/natives"
LIB_DIR="$BASE_DIR/libraries"
ASSETS_DIR="$BASE_DIR/assets"
RESOURCES_DIR="$BASE_DIR/resources"          # legacy flat resources folder
CLIENT_JAR="$BIN_DIR/minecraft.jar"
VERSION_JSON_CACHE="$BASE_DIR/version.json"

MANIFEST_URL="https://launchermeta.mojang.com/mc/game/version_manifest.json"

mkdir -p "$BIN_DIR" "$NATIVES_DIR" "$LIB_DIR" "$ASSETS_DIR" "$RESOURCES_DIR"

for cmd in curl jq unzip java; do
  command -v "$cmd" >/dev/null 2>&1 || { echo "Missing required tool: $cmd" >&2; exit 1; }
done

detect_os() {
  case "$(uname -s)" in
    Linux*)   echo "linux" ;;
    Darwin*)  echo "osx" ;;
    MINGW*|MSYS*|CYGWIN*) echo "windows" ;;
    *)        echo "linux" ;;
  esac
}
OS_NAME="$(detect_os)"

# ---------------------------------------------------------------------------
# 1. Resolve the version JSON (cached after first run)
# ---------------------------------------------------------------------------
if [[ ! -f "$VERSION_JSON_CACHE" ]]; then
  echo "Looking up ${VERSION_ID} in Mojang's version manifest..."
  VERSION_URL=$(curl -fsSL "$MANIFEST_URL" | jq -r --arg id "$VERSION_ID" '.versions[] | select(.id==$id) | .url')
  if [[ -z "$VERSION_URL" || "$VERSION_URL" == "null" ]]; then
    echo "Could not find version $VERSION_ID in manifest." >&2
    exit 1
  fi
  curl -fsSL "$VERSION_URL" -o "$VERSION_JSON_CACHE"
fi

# ---------------------------------------------------------------------------
# 2. Client jar
# ---------------------------------------------------------------------------
if [[ ! -f "$CLIENT_JAR" ]]; then
  echo "Downloading client jar..."
  CLIENT_URL=$(jq -r '.downloads.client.url' "$VERSION_JSON_CACHE")
  curl -fsSL "$CLIENT_URL" -o "$CLIENT_JAR"
else
  echo "Client jar already present, skipping download."
fi

# ---------------------------------------------------------------------------
# 3. Libraries + natives
# ---------------------------------------------------------------------------
CLASSPATH="$CLIENT_JAR"
NUM_LIBS=$(jq '.libraries | length' "$VERSION_JSON_CACHE")

echo "Checking libraries..."
for i in $(seq 0 $((NUM_LIBS - 1))); do
  LIB=$(jq -c ".libraries[$i]" "$VERSION_JSON_CACHE")

  ARTIFACT_URL=$(echo "$LIB" | jq -r '.downloads.artifact.url // empty')
  ARTIFACT_PATH=$(echo "$LIB" | jq -r '.downloads.artifact.path // empty')

  if [[ -n "$ARTIFACT_URL" ]]; then
    DEST="$LIB_DIR/$ARTIFACT_PATH"
    mkdir -p "$(dirname "$DEST")"
    if [[ ! -f "$DEST" ]]; then
      echo "  downloading library: $ARTIFACT_PATH"
      curl -fsSL "$ARTIFACT_URL" -o "$DEST"
    fi
    CLASSPATH="$CLASSPATH:$DEST"
  fi

  # Native (per-OS) classifiers, e.g. natives-linux / natives-windows / natives-osx
  NATIVE_KEY=$(echo "$LIB" | jq -r --arg os "$OS_NAME" '.downloads.classifiers | keys[]? | select(test("natives-" + $os))' 2>/dev/null | head -n1 || true)
  if [[ -n "${NATIVE_KEY:-}" ]]; then
    NATIVE_URL=$(echo "$LIB" | jq -r --arg key "$NATIVE_KEY" '.downloads.classifiers[$key].url')
    NATIVE_PATH=$(echo "$LIB" | jq -r --arg key "$NATIVE_KEY" '.downloads.classifiers[$key].path')
    NATIVE_ZIP="$LIB_DIR/$NATIVE_PATH"
    mkdir -p "$(dirname "$NATIVE_ZIP")"
    if [[ ! -f "$NATIVE_ZIP" ]]; then
      echo "  downloading natives: $NATIVE_PATH"
      curl -fsSL "$NATIVE_URL" -o "$NATIVE_ZIP"
    fi
    unzip -oq "$NATIVE_ZIP" -d "$NATIVES_DIR" -x 'META-INF/*'
  fi
done

# ---------------------------------------------------------------------------
# 4. Legacy assets (sounds/music/lang) — best effort, game runs fine without them
# ---------------------------------------------------------------------------
ASSET_INDEX_URL=$(jq -r '.assetIndex.url // empty' "$VERSION_JSON_CACHE")
if [[ -n "$ASSET_INDEX_URL" ]] && [[ -z "$(ls -A "$RESOURCES_DIR" 2>/dev/null)" ]]; then
  echo "Fetching legacy asset index (this can take a while, safe to Ctrl+C and rerun)..."
  ASSET_INDEX_JSON="$BASE_DIR/asset_index.json"
  curl -fsSL "$ASSET_INDEX_URL" -o "$ASSET_INDEX_JSON" || true

  if [[ -f "$ASSET_INDEX_JSON" ]]; then
    OBJ_DIR="$ASSETS_DIR/objects"
    mkdir -p "$OBJ_DIR"
    jq -r '.objects | to_entries[] | "\(.key)\t\(.value.hash)"' "$ASSET_INDEX_JSON" | \
    while IFS=$'\t' read -r NAME HASH; do
      SUBDIR="${HASH:0:2}"
      OBJ_PATH="$OBJ_DIR/$SUBDIR/$HASH"
      DEST_PATH="$RESOURCES_DIR/$NAME"
      mkdir -p "$(dirname "$DEST_PATH")" "$(dirname "$OBJ_PATH")"
      if [[ ! -f "$OBJ_PATH" ]]; then
        curl -fsSL "https://resources.download.minecraft.net/$SUBDIR/$HASH" -o "$OBJ_PATH" 2>/dev/null || continue
      fi
      cp "$OBJ_PATH" "$DEST_PATH" 2>/dev/null || true
    done
  fi
fi

# ---------------------------------------------------------------------------
# 5. Build and run the launch command
# ---------------------------------------------------------------------------
MAIN_CLASS=$(jq -r '.mainClass' "$VERSION_JSON_CACHE")
ARGS_TEMPLATE=$(jq -r '.minecraftArguments' "$VERSION_JSON_CACHE")

# Offline "session": any non-empty token works because b1.7.3 predates
# Mojang's Yggdrasil auth, and offline-mode servers don't verify sessions anyway.
AUTH_SESSION="notoken:${USERNAME}:0"

ARGS="$ARGS_TEMPLATE"
ARGS="${ARGS//\$\{auth_player_name\}/$USERNAME}"
ARGS="${ARGS//\$\{auth_session\}/$AUTH_SESSION}"
ARGS="${ARGS//\$\{auth_uuid\}/00000000-0000-0000-0000-000000000000}"
ARGS="${ARGS//\$\{auth_access_token\}/0}"
ARGS="${ARGS//\$\{user_type\}/legacy}"
ARGS="${ARGS//\$\{version_name\}/$VERSION_ID}"
ARGS="${ARGS//\$\{game_directory\}/$BASE_DIR}"
ARGS="${ARGS//\$\{assets_root\}/$RESOURCES_DIR}"
ARGS="${ARGS//\$\{game_assets\}/$RESOURCES_DIR}"
ARGS="${ARGS//\$\{assets_index_name\}/legacy}"
ARGS="${ARGS//\$\{user_properties\}/\{\}}"

echo
echo "Launching Minecraft ${VERSION_ID} as '${USERNAME}'..."
echo

# shellcheck disable=SC2086
exec ./x64_jdk1.8.0_202/bin/java -Djava.library.path="$NATIVES_DIR" \
     -Dorg.lwjgl.librarypath="$NATIVES_DIR" \
     -Dnet.java.games.input.librarypath="$NATIVES_DIR" \
     -cp "$CLASSPATH" \
     "$MAIN_CLASS" $ARGS
