#!/bin/sh
# Publishes one build artifact from CI (both workflows call it, each with its own file): every push to main
# is the release "MyPods X.Y.N" with the tag vX.Y.N on that commit, the version the app shows (CMakeLists.txt):
# MAJOR.MINOR plus the commits since it was set.
# Needs GH_TOKEN and GITHUB_SHA (both set by GitHub Actions).
set -eu
file=$1
version=$(sed -n 's/^project(mypods VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt).$(git rev-list --count \
    "$(git log -1 --format=%H -G'^project.mypods VERSION' -- CMakeLists.txt)"..HEAD)

tag=v$version

# both workflows try to create it; the second one finds it there
gh release create "$tag" --target "$GITHUB_SHA" --title "MyPods $version" --generate-notes || gh release view "$tag" >/dev/null
gh release upload "$tag" "$file" --clobber
