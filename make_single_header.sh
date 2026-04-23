#!/usr/bin/env bash
# make_single_header.sh
# Generates single_include/gaia.h from include/gaia.h.
#
# Uses a pure shell include-walker: only #include directives that resolve to a
# file under include/ are inlined. Everything else — system headers, external
# dependencies, preprocessor definitions, conditionals — passes through verbatim.
#
# The output can be formatted with clang-format using the project's
# .clang-format config (via --style=file).
# Formatting stays enabled by default when clang-format is available.
#
# Usage:
#   ./make_single_header.sh [--format|--no-format] [clang-format-executable]
#
# Examples:
#   ./make_single_header.sh                          # default: format when clang-format is available
#   ./make_single_header.sh clang-format-17          # same, but with an explicit formatter
#   ./make_single_header.sh --format clang-format-17 # explicit format request
#   ./make_single_header.sh --no-format              # skip formatting

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")" && pwd)"
INPUT="$REPO_ROOT/include/gaia.h"
OUTPUT="$REPO_ROOT/single_include/gaia.h"
INCLUDE_DIR="$REPO_ROOT/include"

# ---------------------------------------------------------------------------
# Resolve clang-format
# ---------------------------------------------------------------------------
print_usage() {
    cat <<'EOF'
Usage:
  ./make_single_header.sh [--format|--no-format] [clang-format-executable]

Options:
  --format      Enable formatting (default when clang-format is available).
  --no-format   Skip formatting.
  -h, --help    Print this help.
EOF
}

parse_args() {
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --format)
                FORMAT_MODE="on"
                ;;
            --no-format)
                FORMAT_MODE="off"
                ;;
            -h|--help)
                print_usage
                exit 0
                ;;
            *)
                if [[ -n "$CLANG_FORMAT_ARG" ]]; then
                    echo "ERROR: unexpected argument '$1'." >&2
                    print_usage >&2
                    exit 1
                fi
                CLANG_FORMAT_ARG="$1"
                ;;
        esac
        shift
    done
}

resolve_clang_format() {
    local arg="${1:-}"

    if [[ "$FORMAT_MODE" == "off" ]]; then
        CLANG_FORMAT=""
        return
    fi

    if [[ -n "$arg" ]]; then
        if ! command -v "$arg" &>/dev/null; then
            echo "ERROR: '$arg' not found on PATH." >&2
            exit 1
        fi
        CLANG_FORMAT="$arg"
        return
    fi

    for candidate in clang-format clang-format-19 clang-format-18 clang-format-17; do
        if command -v "$candidate" &>/dev/null; then
            CLANG_FORMAT="$candidate"
            return
        fi
    done

    CLANG_FORMAT=""
}

# ---------------------------------------------------------------------------
# Shell include-walker
#
# Rules:
#   - #include "..." or #include <...> that resolves under INCLUDE_DIR -> inline
#   - Everything else passes through verbatim (system headers, #define, #if, etc.)
#   - #pragma once stripped from inlined files (output has its own at the top)
#   - Already-visited files skipped (honours #pragma once semantics)
# ---------------------------------------------------------------------------
walker() {
    local file
    file="$(realpath "$1")"

    # Already visited — skip (pragma once semantics)
    if grep -qF "$file" "$VISITED_FILE" 2>/dev/null; then
        return
    fi
    echo "$file" >> "$VISITED_FILE"

    local dir
    dir="$(dirname "$file")"

    while IFS= read -r line || [[ -n "$line" ]]; do

        # Strip #pragma once from inlined files
        if [[ "$line" =~ ^[[:space:]]*#[[:space:]]*pragma[[:space:]]+once[[:space:]]*$ ]]; then
            continue
        fi

        # Check for a #include directive
        if [[ "$line" =~ ^[[:space:]]*#[[:space:]]*include[[:space:]]*([\"<])([^\"\>]+)[\"\>] ]]; then
            local delimiter="${BASH_REMATCH[1]}"
            local inc="${BASH_REMATCH[2]}"
            local resolved=""

            # Angle-bracket includes (<...>) are always system/external — never inline.
            if [[ "$delimiter" == "<" ]]; then
                echo "$line"
                continue
            fi

            # Quoted includes ("...") — attempt to resolve inside INCLUDE_DIR.

            # 1. Relative to the current file
            if [[ -f "$dir/$inc" ]]; then
                local cand
                cand="$(realpath "$dir/$inc")"
                [[ "$cand" == "$INCLUDE_DIR"* ]] && resolved="$cand"
            fi

            # 2. Relative to INCLUDE_DIR
            if [[ -z "$resolved" && -f "$INCLUDE_DIR/$inc" ]]; then
                resolved="$(realpath "$INCLUDE_DIR/$inc")"
            fi

            # 3. Recursive search under INCLUDE_DIR by basename.
            #    Catches cases where a header is included by filename only (e.g.
            #    "version.h") but lives at a subdirectory path inside INCLUDE_DIR
            #    (e.g. include/gaia/config/version.h).
            if [[ -z "$resolved" ]]; then
                local base="${inc##*/}"
                local found
                found="$(find "$INCLUDE_DIR" -name "$base" 2>/dev/null | head -1)"
                if [[ -n "$found" ]]; then
                    resolved="$(realpath "$found")"
                fi
            fi

            if [[ -n "$resolved" ]]; then
                walker "$resolved"   # internal -> recurse
            else
                echo "$line"         # external -> keep verbatim
            fi
        else
            echo "$line"             # everything else -> keep verbatim
        fi

    done < "$file"
}

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
CLANG_FORMAT=""
CLANG_FORMAT_ARG=""
FORMAT_MODE="auto"
VISITED_FILE=""
parse_args "$@"
resolve_clang_format "$CLANG_FORMAT_ARG"

VISITED_FILE="$(mktemp)"
trap 'rm -f "$VISITED_FILE"' EXIT

echo "Input        : $INPUT"
echo "Output       : $OUTPUT"
if [[ -n "$CLANG_FORMAT" ]]; then
    echo "clang-format : $CLANG_FORMAT"
elif [[ "$FORMAT_MODE" == "off" ]]; then
    echo "clang-format : disabled"
else
    echo "clang-format : not found - formatting will be skipped"
fi

mkdir -p "$(dirname "$OUTPUT")"
rm -f "$OUTPUT"

{
    echo "// Amalgamated single-header build of Gaia-ECS."
    echo "// The file is generated. Do not edit it."
    echo "#pragma once"
    echo ""
    walker "$INPUT"
} > "$OUTPUT"

if [[ -n "$CLANG_FORMAT" ]]; then
    echo "Formatting   : $OUTPUT"
    "$CLANG_FORMAT" -i --style=file "$OUTPUT"
else
    echo "Formatting   : skipped"
fi

echo "Done: $OUTPUT ($(wc -l < "$OUTPUT") lines)"
