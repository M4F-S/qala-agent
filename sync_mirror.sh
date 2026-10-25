#!/usr/bin/env bash
# sync_mirror.sh - Automated Tri-Mirror Synchronization for Qala Agent
# Syncs between: 1. Local Workspace, 2. Google Drive Mirror, 3. GitHub Remote

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GDRIVE_DIR="/Users/mohamedfathy/Library/CloudStorage/GoogleDrive-mmmmfathy7@gmail.com/My Drive/Dubai_AI_Championship_2027/qala-agent"

echo "=================================================================="
echo " QALA AGENT: TRI-MIRROR SYNCHRONIZATION"
echo "=================================================================="

# 1. Clean local build artifacts before syncing
echo "[1/3] Cleaning local build artifacts..."
make -C "$SCRIPT_DIR" clean >/dev/null 2>&1 || true

# 2. Sync to Google Drive Mirror
if [[ -d "/Users/mohamedfathy/Library/CloudStorage/GoogleDrive-mmmmfathy7@gmail.com/My Drive/Dubai_AI_Championship_2027" ]]; then
    echo "[2/3] Mirroring to Google Drive..."
    mkdir -p "$GDRIVE_DIR"
    rsync -av --delete --exclude=".git" --exclude="build" --exclude="*.dylib" --exclude="*.so" "$SCRIPT_DIR/" "$GDRIVE_DIR/"
    echo "[PASS] Google Drive mirror updated."
else
    echo "[WARN] Google Drive directory not mounted. Skipping cloud sync."
fi

# 3. Commit and Push to GitHub Remote
echo "[3/3] Checking Git status and pushing to GitHub..."
cd "$SCRIPT_DIR"
git add .
if ! git diff --cached --quiet; then
    COMMIT_MSG="${1:-chore: synchronize qala-agent codebase and documentation}"
    git commit -m "$COMMIT_MSG"
    git push origin main
    echo "[PASS] GitHub remote updated."
else
    echo "[INFO] Git working tree clean. Nothing to commit."
fi

echo "=================================================================="
echo " [SUCCESS] Tri-Mirror Synchronization Complete!"
echo "=================================================================="
