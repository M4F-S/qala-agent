#!/usr/bin/env bash
# sync_mirror.sh - Automated Tri-Mirror Synchronization for Qala Agent
# Syncs between: 1. Local Workspace, 2. Google Drive Mirror, 3. GitHub Remote

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Auto-detect Google Drive mount if not explicitly parameterized
DEFAULT_GDRIVE_ROOT=""
for candidate in "$HOME/Library/CloudStorage"/GoogleDrive*"/My Drive/Dubai_AI_Championship_2027"; do
    if [[ -d "$candidate" ]]; then
        DEFAULT_GDRIVE_ROOT="$candidate"
        break
    fi
done

GDRIVE_ROOT="${GDRIVE_ROOT:-$DEFAULT_GDRIVE_ROOT}"
GDRIVE_DIR="${GDRIVE_ROOT:+$GDRIVE_ROOT/qala-agent}"

echo "=================================================================="
echo " QALA AGENT: TRI-MIRROR SYNCHRONIZATION"
echo "=================================================================="

# 1. Clean local build artifacts before syncing
echo "[1/3] Cleaning local build artifacts..."
make -C "$SCRIPT_DIR" clean >/dev/null 2>&1 || true

# 2. Sync to Google Drive Mirror
if [[ -n "$GDRIVE_ROOT" && -d "$GDRIVE_ROOT" ]]; then
    echo "[2/3] Mirroring to Google Drive ($GDRIVE_DIR)..."
    mkdir -p "$GDRIVE_DIR"
    RSYNC_OPTS=(-av --exclude=".git" --exclude="build" --exclude="*.dylib" --exclude="*.so" --exclude="*.o" --exclude="*.jsonl" --exclude="__pycache__")
    if [[ "${FORCE:-0}" == "1" ]]; then
        RSYNC_OPTS+=(--delete)
    fi
    rsync "${RSYNC_OPTS[@]}" "$SCRIPT_DIR/" "$GDRIVE_DIR/"
    echo "[PASS] Google Drive mirror updated."
else
    echo "[WARN] Google Drive root not found or not mounted. Set GDRIVE_ROOT to override. Skipping cloud sync."
fi

# 3. Commit and Push to GitHub Remote
echo "[3/3] Checking Git status and pushing to GitHub..."
cd "$SCRIPT_DIR"

EXPLICIT_PATHS=(
    "adapters"
    "src"
    "tests"
    "corpus"
    "docs"
    ".github"
    "Makefile"
    "README.md"
    "PLAN.md"
    "PROJECT_RULES.md"
    "AGENT_INSTRUCTIONS.md"
    "LICENSE"
    "NOTICE"
    "sync_mirror.sh"
    ".gitignore"
)

for p in "${EXPLICIT_PATHS[@]}"; do
    if [[ -e "$SCRIPT_DIR/$p" ]]; then
        git add "$SCRIPT_DIR/$p"
    fi
done

# Abort if temporary build or audit files were inadvertently staged
if git diff --cached --name-only | grep -E "(^|/)(build/|__pycache__/|\.o$|\.so$|\.dylib$|\.jsonl$)" >/dev/null; then
    echo "[ERROR] Refusing to commit: staged files contain temporary build/audit artifacts:"
    git diff --cached --name-only | grep -E "(^|/)(build/|__pycache__/|\.o$|\.so$|\.dylib$|\.jsonl$)"
    exit 1
fi

if ! git diff --cached --quiet; then
    COMMIT_MSG="${1:-chore: synchronize qala-agent codebase and documentation}"
    git commit -m "$COMMIT_MSG"
    CURRENT_BRANCH="$(git rev-parse --abbrev-ref HEAD)"
    git push origin "$CURRENT_BRANCH"
    echo "[PASS] GitHub remote updated ($CURRENT_BRANCH)."
else
    echo "[INFO] Git working tree clean. Nothing to commit."
fi

echo "=================================================================="
echo " [SUCCESS] Tri-Mirror Synchronization Complete!"
echo "=================================================================="
