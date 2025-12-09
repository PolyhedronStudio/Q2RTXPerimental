#!/bin/bash
# Script to push wiki migration changes to GitHub wiki repository
# Run this script to complete the wiki migration

set -e

WIKI_REPO_DIR="/home/runner/work/Q2RTXPerimental/wiki-repo"

echo "=================================================="
echo "GitHub Wiki Migration - Push Script"
echo "=================================================="
echo ""

# Check if wiki-repo directory exists
if [ ! -d "$WIKI_REPO_DIR" ]; then
    echo "ERROR: Wiki repository not found at $WIKI_REPO_DIR"
    echo "Please ensure the wiki migration preparation has been completed."
    exit 1
fi

cd "$WIKI_REPO_DIR"

# Check if there are committed changes ready to push
if [ -z "$(git log origin/master..master 2>/dev/null)" ]; then
    echo "ERROR: No commits to push. The changes may have already been pushed."
    exit 1
fi

echo "Summary of changes to be pushed:"
echo "--------------------------------"
git log origin/master..master --oneline
echo ""

echo "Files to be added/modified:"
echo "-------------------------"
git diff --name-status origin/master..master
echo ""

# Attempt to push
echo "Attempting to push to GitHub wiki repository..."
echo ""

if git push origin master; then
    echo ""
    echo "✅ SUCCESS! Wiki migration completed successfully!"
    echo ""
    echo "Your wiki has been updated with:"
    echo "  - 33 comprehensive documentation pages"
    echo "  - Organized navigation sidebar (_Sidebar.md)"
    echo "  - Footer with links (_Footer.md)"
    echo "  - Backup of old wiki content (Old-Wiki-*.md)"
    echo ""
    echo "Visit your wiki at: https://github.com/PolyhedronStudio/Q2RTXPerimental/wiki"
else
    echo ""
    echo "❌ Push failed. This is likely due to authentication issues."
    echo ""
    echo "To complete the push manually:"
    echo "1. Navigate to the wiki repository:"
    echo "   cd $WIKI_REPO_DIR"
    echo ""
    echo "2. Configure git credentials if needed:"
    echo "   git config credential.helper store"
    echo "   # or use GitHub CLI: gh auth login"
    echo ""
    echo "3. Push the changes:"
    echo "   git push origin master"
    echo ""
    echo "Alternatively, you can clone the wiki separately and copy files:"
    echo "1. Clone wiki: git clone https://github.com/PolyhedronStudio/Q2RTXPerimental.wiki.git"
    echo "2. Copy all .md files from $WIKI_REPO_DIR to your cloned wiki"
    echo "3. Commit and push from your local clone"
    exit 1
fi
