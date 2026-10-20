#!/bin/bash
# prepare_release.sh — the version number and release notes for a new
# release, used by .github/workflows/prepare-release.yml.  The version is
# only the git tag: the program reads it with git describe, so no file in the
# repository carries the number.
#
# Usage:
#   ./prepare_release.sh next <novidade|correcao|grande|vX.Y[.Z]>
#       print the version after the newest one in CHANGELOG.md: novidade
#       v1.0.5 -> v1.1, correcao v1.1 -> v1.1.1, grande v1.1 -> v2.0,
#       or check and print the given version
#   ./prepare_release.sh changelog <version>
#       turn "## Next version" in CHANGELOG.md into "## <version> (<date>)"
#   ./prepare_release.sh current
#       print the version of the newest section of CHANGELOG.md (empty when
#       it is still "Next version")
#   ./prepare_release.sh notes <version>
#       print release notes for <version>: its CHANGELOG.md section, with the
#       wrapped lines joined (GitHub keeps line breaks in release notes)
set -euo pipefail

REPO="https://github.com/gabrielbadolato/tyrian2000plus"
cd "$(dirname "$0")"

die() { echo "prepare_release.sh: $*" >&2; exit 1; }

valid_version() { [[ "$1" =~ ^v[0-9]+\.[0-9]+(\.[0-9]+)?$ ]]; }

# The newest released version is the newest "## vX.Y (date)" section of
# CHANGELOG.md (the tags also hold the parent projects' own versions).
latest_version() {
    sed -nE 's/^## (v[0-9]+\.[0-9]+(\.[0-9]+)?) .*/\1/p' CHANGELOG.md | head -n 1
}

next_version() {
    local kind="$1" latest major minor patch
    if [[ "$kind" == v* ]]; then
        valid_version "$kind" || die "not a version like v1.2 or v1.2.3: $kind"
        echo "$kind"
        return
    fi
    latest=$(latest_version)
    [ -n "$latest" ] || die "no released version in CHANGELOG.md"
    IFS=. read -r major minor patch <<< "${latest#v}"
    patch=${patch:-0}
    case "$kind" in
        novidade) minor=$((minor + 1)); patch=0 ;;
        correcao|correção) patch=$((patch + 1)) ;;
        grande) major=$((major + 1)); minor=0; patch=0 ;;
        *) die "unknown kind: $kind (novidade, correcao, grande or vX.Y)" ;;
    esac
    if [ "$patch" = 0 ]; then
        echo "v$major.$minor"
    else
        echo "v$major.$minor.$patch"
    fi
}

cmd="${1:-}"
case "$cmd" in
    next)
        version=$(next_version "${2:?kind or version}")
        if git rev-parse -q --verify "refs/tags/$version" > /dev/null; then
            die "$version already exists"
        fi
        echo "$version"
        ;;
    changelog)
        version="${2:?version}"
        valid_version "$version" || die "not a version: $version"
        grep -q '^## Next version *$' CHANGELOG.md ||
            die 'CHANGELOG.md has no "## Next version" section'
        # The section needs at least one entry before the next "## ".
        awk '/^## Next version *$/ { s = 1; next } /^## / { s = 0 }
             s && /^- / { found = 1 } END { exit !found }' CHANGELOG.md ||
            die '"## Next version" in CHANGELOG.md is empty'
        date=$(TZ=America/Sao_Paulo date +%F)
        sed -i "0,/^## Next version *\$/s//## $version ($date)/" CHANGELOG.md
        ;;
    current)
        grep -m 1 '^## ' CHANGELOG.md |
            sed -nE 's/^## (v[0-9]+\.[0-9]+(\.[0-9]+)?) .*/\1/p'
        ;;
    notes)
        version="${2:?version}"
        valid_version "$version" || die "not a version: $version"
        # The lines between "## <version> " and the next "## ", wrapped
        # lines joined to the line they continue.
        awk -v v="$version" '
            index($0, "## " v " ") == 1 { s = 1; next }
            s && /^## / { exit }
            !s { next }
            /^$/ { flush(); blank = 1; next }
            line != "" && !/^ *- / && !/^#/ {
                sub(/^ +/, ""); line = line " " $0; next
            }
            { flush(); line = $0 }
            function flush() {
                if (line == "") return
                if (blank && printed) print ""
                print line; line = ""; blank = 0; printed = 1
            }
            END { flush() }' CHANGELOG.md > "${TMPDIR:-/tmp}/notes.$$"
        [ -s "${TMPDIR:-/tmp}/notes.$$" ] ||
            { rm -f "${TMPDIR:-/tmp}/notes.$$"; die "no $version section in CHANGELOG.md"; }
        echo "**English**"
        echo
        cat "${TMPDIR:-/tmp}/notes.$$"
        rm -f "${TMPDIR:-/tmp}/notes.$$"
        echo
        echo "Full list: $REPO/blob/$version/CHANGELOG.md"
        echo
        echo "<!-- Português: as notas em português podem ser coladas aqui"
        echo "     antes de publicar (peça a tradução numa thread). -->"
        ;;
    *)
        sed -n '2,/^set /p' "$0" | sed '$d; s/^# \{0,1\}//'
        exit 1
        ;;
esac
