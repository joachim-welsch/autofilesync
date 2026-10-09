#!/bin/bash
# AutoFileSync chat model setup (macOS)
# Provides a GGUF model and publishes AUTOFILESYNC_LLAMA_MODEL so that
# LlamaClient finds it - including .app bundles started from the Finder, which
# inherit neither .zshrc nor .zprofile.

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

MODEL_DIR="${HOME}/Library/Application Support/autofilesync/models"
AGENT_PLIST="${HOME}/Library/LaunchAgents/com.autofilesync.llamamodel.plist"
VARIABLE="AUTOFILESYNC_LLAMA_MODEL"
REPO=""
FILE=""
MODEL_FILE=""
VERIFY=true

while [[ $# -gt 0 ]]; do
    case $1 in
        --size)
            case $2 in
                3b)  REPO="bartowski/Qwen2.5-Coder-3B-Instruct-GGUF";  FILE="Qwen2.5-Coder-3B-Instruct-Q4_K_M.gguf" ;;
                7b)  REPO="bartowski/Qwen2.5-Coder-7B-Instruct-GGUF";  FILE="Qwen2.5-Coder-7B-Instruct-Q4_K_M.gguf" ;;
                14b) REPO="bartowski/Qwen2.5-Coder-14B-Instruct-GGUF"; FILE="Qwen2.5-Coder-14B-Instruct-Q4_K_M.gguf" ;;
                *) echo -e "${RED}--size expects 3b, 7b or 14b${NC}"; exit 1 ;;
            esac
            shift 2 ;;
        --model) MODEL_FILE="$2"; shift 2 ;;
        --no-verify) VERIFY=false; shift ;;
        -h|--help)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --size 3b|7b|14b  Model size (default: chosen from installed RAM)"
            echo "  --model FILE      Use an existing GGUF file instead of downloading"
            echo "  --no-verify       Skip the GGUF integrity check"
            echo "  -h, --help        Show this help message"
            exit 0 ;;
        *) echo -e "${RED}Unknown option: $1${NC}"; exit 1 ;;
    esac
done

[[ "$(uname -s)" == "Darwin" ]] || { echo -e "${RED}This script only runs on macOS${NC}"; exit 1; }

RAM_GB=$(( $(sysctl -n hw.memsize) / 1073741824 ))
# LlamaClient loads with n_gpu_layers = 0, so the chat runs on the CPU and a
# larger model costs answer latency, not just memory.
if [[ -z "$REPO" && -z "$MODEL_FILE" ]]; then
    if   (( RAM_GB >= 32 )); then REPO="bartowski/Qwen2.5-Coder-14B-Instruct-GGUF"; FILE="Qwen2.5-Coder-14B-Instruct-Q4_K_M.gguf"
    elif (( RAM_GB >= 16 )); then REPO="bartowski/Qwen2.5-Coder-7B-Instruct-GGUF";  FILE="Qwen2.5-Coder-7B-Instruct-Q4_K_M.gguf"
    else                          REPO="bartowski/Qwen2.5-Coder-3B-Instruct-GGUF";  FILE="Qwen2.5-Coder-3B-Instruct-Q4_K_M.gguf"
    fi
fi
echo -e "${GREEN}Machine:${NC} $(sysctl -n machdep.cpu.brand_string), ${RAM_GB} GB RAM"

if [[ -n "$MODEL_FILE" ]]; then
    [[ -f "$MODEL_FILE" ]] || { echo -e "${RED}No such file: ${MODEL_FILE}${NC}"; exit 1; }
    MODEL_FILE="$(cd "$(dirname "$MODEL_FILE")" && pwd)/$(basename "$MODEL_FILE")"
else
    mkdir -p "$MODEL_DIR"
    MODEL_FILE="${MODEL_DIR}/${FILE}"
    CACHED=$(find "${HOME}/.cache/huggingface/hub" -name "$FILE" -type f 2>/dev/null | head -1)
    if [[ -f "$MODEL_FILE" ]]; then
        echo -e "${GREEN}Model already present:${NC} ${MODEL_FILE}"
    elif [[ -n "$CACHED" ]]; then
        # A previous llama.cpp download already fetched these gigabytes.
        echo -e "${YELLOW}Linking model from the Hugging Face cache...${NC}"
        ln -s "$CACHED" "$MODEL_FILE"
    else
        echo -e "${YELLOW}Downloading ${FILE}...${NC}"
        curl -L -C - --fail --progress-bar -o "${MODEL_FILE}.part" \
            "https://huggingface.co/${REPO}/resolve/main/${FILE}"
        mv "${MODEL_FILE}.part" "$MODEL_FILE"
    fi
fi
echo -e "${GREEN}Model:${NC} ${MODEL_FILE}"

if [[ "$VERIFY" == true ]]; then
    # A load test through llama-cli would cost minutes on the CPU backend, so
    # check what actually goes wrong instead: a truncated or HTML error-page
    # download. GGUF files start with the magic "GGUF".
    [[ "$(head -c 4 "$MODEL_FILE")" == "GGUF" ]] \
        || { echo -e "${RED}Not a GGUF file: ${MODEL_FILE}${NC}"; exit 1; }
    SIZE=$(stat -Lf %z "$MODEL_FILE")
    (( SIZE > 1000000000 )) \
        || { echo -e "${RED}Model looks truncated (${SIZE} bytes)${NC}"; exit 1; }
    echo -e "${GREEN}Model verified${NC} ($(( SIZE / 1073741824 )) GB)"
fi

# Three places, because each covers a different way of starting the program:
# launchctl for the running GUI session, the LaunchAgent for sessions after a
# reboot, the shell rc for starts from a terminal.
launchctl setenv "$VARIABLE" "$MODEL_FILE"
cat > "$AGENT_PLIST" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>Label</key><string>com.autofilesync.llamamodel</string>
    <key>ProgramArguments</key>
    <array>
        <string>/bin/launchctl</string>
        <string>setenv</string>
        <string>${VARIABLE}</string>
        <string>${MODEL_FILE}</string>
    </array>
    <key>RunAtLoad</key><true/>
</dict>
</plist>
PLIST
plutil -lint "$AGENT_PLIST" >/dev/null
for RC in "${HOME}/.zshrc" "${HOME}/.zprofile"; do
    [[ -f "$RC" ]] || continue
    grep -q "$VARIABLE" "$RC" && continue
    printf '\nexport %s="%s"\n' "$VARIABLE" "$MODEL_FILE" >> "$RC"
done

echo -e "${GREEN}Done.${NC} ${VARIABLE} is set for this login session and after a reboot."
echo -e "${YELLOW}An already running autofilesync must be restarted to pick it up.${NC}"
