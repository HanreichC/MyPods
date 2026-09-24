#!/bin/sh
# Publishes one build artifact from CI (both workflows call it, each with its own file):
#   a tag vX.Y.Z  -> the release "MyPods X.Y.Z" (the tag has to match the version in CMakeLists.txt)
#   main          -> the rolling pre-release "nightly", moved to this commit
# Needs GH_TOKEN, GITHUB_REF, GITHUB_SHA and GITHUB_REPOSITORY (all set by GitHub Actions).
set -eu
file=$1

case "$GITHUB_REF" in
refs/tags/v*)
    tag=${GITHUB_REF#refs/tags/}
    if ! grep -q "^project(mypods VERSION ${tag#v} " CMakeLists.txt; then
        echo "::error::Tag $tag does not match the version in CMakeLists.txt"
        exit 1
    fi
    # both workflows try to create it; the second one finds it there
    gh release create "$tag" --verify-tag --title "MyPods ${tag#v}" --generate-notes || gh release view "$tag" >/dev/null
    gh release upload "$tag" "$file" --clobber
    ;;
refs/heads/main)
    gh api -X PATCH "repos/$GITHUB_REPOSITORY/git/refs/tags/nightly" -f sha="$GITHUB_SHA" -F force=true >/dev/null 2>&1 ||
        gh api -X POST "repos/$GITHUB_REPOSITORY/git/refs" -f ref=refs/tags/nightly -f sha="$GITHUB_SHA" >/dev/null 2>&1 || true
    gh release create nightly --prerelease --title "MyPods nightly" \
        --notes "Built from the latest commit on main, replaced with every push. Stable versions are the other releases." ||
        gh release view nightly >/dev/null
    gh release upload nightly "$file" --clobber
    ;;
esac
