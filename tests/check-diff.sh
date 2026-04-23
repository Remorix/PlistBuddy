#!/bin/sh

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO_ROOT=$(CDPATH= cd -- "${SCRIPT_DIR}/.." && pwd)

OURS=${OURS:-${REPO_ROOT}/PlistBuddy}
SOURCE=${SOURCE:-/usr/libexec/PlistBuddy}
TEMP_REFERENCE=$(mktemp -d "${TMPDIR:-/tmp}/plistbuddy-diff-reference.XXXXXX")

cleanup() {
	if [ "${KEEP_TMP:-0}" != "1" ]; then
		rm -rf "${TEMP_REFERENCE}"
	fi
}

trap cleanup EXIT INT TERM

GENERATE_REFERENCE=1 \
SOURCE="${SOURCE}" \
REFERENCE_DIR="${TEMP_REFERENCE}" \
sh "${SCRIPT_DIR}/regression.sh" >/dev/null

OURS="${OURS}" \
REFERENCE_DIR="${TEMP_REFERENCE}" \
sh "${SCRIPT_DIR}/regression.sh"
