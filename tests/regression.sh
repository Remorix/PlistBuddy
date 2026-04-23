#!/bin/sh

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO_ROOT=$(CDPATH= cd -- "${SCRIPT_DIR}/.." && pwd)

OURS=${OURS:-${REPO_ROOT}/PlistBuddy}
SOURCE=${SOURCE:-/usr/libexec/PlistBuddy}
REFERENCE_DIR=${REFERENCE_DIR:-${SCRIPT_DIR}/reference}
GENERATE_REFERENCE=${GENERATE_REFERENCE:-0}
ROOT=$(mktemp -d "${TMPDIR:-/tmp}/plistbuddy-regression.XXXXXX")
FIX="${ROOT}/fixtures"
FAILED_CASES="${ROOT}/failed_cases.txt"
PASS=0
FAIL=0

case "${OURS}" in
/*) ;;
*) OURS=${REPO_ROOT}/${OURS} ;;
esac

case "${SOURCE}" in
/*) ;;
*) SOURCE=${REPO_ROOT}/${SOURCE} ;;
esac

cleanup() {
	if [ "${KEEP_TMP:-0}" != "1" ]; then
		rm -rf "${ROOT}"
	fi
}

trap cleanup EXIT INT TERM

if [ "${GENERATE_REFERENCE}" = "1" ]; then
	if [ ! -x "${SOURCE}" ]; then
		printf 'missing reference source PlistBuddy: %s\n' "${SOURCE}" >&2
		exit 1
	fi
	mkdir -p "${REFERENCE_DIR}"
else
	if [ ! -x "${OURS}" ]; then
		printf 'missing built PlistBuddy: %s\n' "${OURS}" >&2
		exit 1
	fi
	if [ ! -d "${REFERENCE_DIR}" ]; then
		printf 'missing reference set: %s\n' "${REFERENCE_DIR}" >&2
		exit 1
	fi
fi

mkdir -p "${FIX}"
: > "${FAILED_CASES}"

cat > "${FIX}/template-dict.plist" <<'EOF_TEMPLATE_DICT'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>root</key>
    <dict>
        <key>a</key><string>one</string>
        <key>arr</key>
        <array>
            <string>x</string>
        </array>
    </dict>
    <key>plain</key><string>text</string>
</dict>
</plist>
EOF_TEMPLATE_DICT

cat > "${FIX}/template-array.plist" <<'EOF_TEMPLATE_ARRAY'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<array>
    <string>a</string>
    <string>b</string>
</array>
</plist>
EOF_TEMPLATE_ARRAY

cat > "${FIX}/merge-dict.plist" <<'EOF_MERGE_DICT'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>b</key><string>two</string>
    <key>arr</key>
    <array>
        <string>y</string>
    </array>
</dict>
</plist>
EOF_MERGE_DICT

cat > "${FIX}/merge-array.plist" <<'EOF_MERGE_ARRAY'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<array>
    <string>m1</string>
    <string>m2</string>
</array>
</plist>
EOF_MERGE_ARRAY

printf 'ABC\0DEF' > "${FIX}/blob.bin"
printf 'Quit\n' > "${FIX}/stdin-quit.txt"
printf 'Bye\n' > "${FIX}/stdin-bye.txt"
printf '?\nExit\n' > "${FIX}/stdin-help.txt"
printf 'Print :root\nExit\n' > "${FIX}/stdin-print.txt"
: > "${FIX}/stdin-empty.txt"

record_pass() {
	PASS=$((PASS + 1))
}

record_fail() {
	FAIL=$((FAIL + 1))
	printf '%s :: %s\n' "$1" "$2" >> "${FAILED_CASES}"
}

append_case_failure() {
	if [ -z "${CASE_FAILURES}" ]; then
		CASE_FAILURES=$1
	else
		CASE_FAILURES="${CASE_FAILURES}; $1"
	fi
}

new_case() {
	CASE_NAME=$1
	PLIST_MODE=$2
	CASE_DIR="${ROOT}/cases/${CASE_NAME}"
	REF_CASE_DIR="${REFERENCE_DIR}/${CASE_NAME}"
	RUN_DIR="${CASE_DIR}/run"
	CASE_FAILURES=
	rm -rf "${CASE_DIR}"
	mkdir -p "${RUN_DIR}"
	cp "${FIX}"/* "${RUN_DIR}/"

	case "${PLIST_MODE}" in
	missing)
		rm -f "${RUN_DIR}/case.plist" "${RUN_DIR}/missing.plist"
		;;
	template-dict)
		cp "${RUN_DIR}/template-dict.plist" "${RUN_DIR}/case.plist"
		;;
	template-array)
		cp "${RUN_DIR}/template-array.plist" "${RUN_DIR}/case.plist"
		;;
	*)
		printf 'unknown plist mode: %s\n' "${PLIST_MODE}" >&2
		exit 1
		;;
	esac

	if [ "${GENERATE_REFERENCE}" = "1" ]; then
		rm -rf "${REF_CASE_DIR}"
		mkdir -p "${REF_CASE_DIR}"
	fi
}

run_one() {
	bin=$1
	dir=$2
	prefix=$3
	stdin_file=$4
	shift 4

	if [ -n "${stdin_file}" ]; then
		if (cd "${dir}" && TZ=UTC LC_ALL=C LANG=C "${bin}" "$@") < "${stdin_file}" > "${prefix}.stdout" 2> "${prefix}.stderr"; then
			status=0
		else
			status=$?
		fi
	else
		if (cd "${dir}" && TZ=UTC LC_ALL=C LANG=C "${bin}" "$@") > "${prefix}.stdout" 2> "${prefix}.stderr"; then
			status=0
		else
			status=$?
		fi
	fi

	printf '%s\n' "${status}" > "${prefix}.exit"
}

compare_streams() {
	if [ ! -f "${REF_CASE_DIR}/run.exit" ] || [ ! -f "${REF_CASE_DIR}/run.stdout" ] || [ ! -f "${REF_CASE_DIR}/run.stderr" ]; then
		append_case_failure "reference streams missing"
		return
	fi

	if ! cmp -s "${REF_CASE_DIR}/run.exit" "${RUN_DIR}/run.exit"; then
		append_case_failure "exit status differs"
	fi

	if ! cmp -s "${REF_CASE_DIR}/run.stdout" "${RUN_DIR}/run.stdout"; then
		append_case_failure "stdout differs"
	fi

	if ! cmp -s "${REF_CASE_DIR}/run.stderr" "${RUN_DIR}/run.stderr"; then
		append_case_failure "stderr differs"
	fi
}

compare_file_exact() {
	rel=$1
	ref_path="${REF_CASE_DIR}/${rel}"
	run_path="${RUN_DIR}/${rel}"

	if [ -e "${ref_path}" ] && [ -e "${run_path}" ]; then
		if ! cmp -s "${ref_path}" "${run_path}"; then
			append_case_failure "file differs: ${rel}"
		fi
		return
	fi

	if [ -e "${ref_path}" ] || [ -e "${run_path}" ]; then
		append_case_failure "file presence differs: ${rel}"
	fi
}

finish_case() {
	case_name=$1

	if [ "${GENERATE_REFERENCE}" = "1" ]; then
		record_pass
		return
	fi

	if [ -n "${CASE_FAILURES}" ]; then
		record_fail "${case_name}" "${CASE_FAILURES}"
	else
		record_pass
	fi
}

write_reference_streams() {
	cp "${RUN_DIR}/run.stdout" "${REF_CASE_DIR}/run.stdout"
	cp "${RUN_DIR}/run.stderr" "${REF_CASE_DIR}/run.stderr"
	cp "${RUN_DIR}/run.exit" "${REF_CASE_DIR}/run.exit"
}

write_reference_file() {
	rel=$1
	if [ -e "${RUN_DIR}/${rel}" ]; then
		cp "${RUN_DIR}/${rel}" "${REF_CASE_DIR}/${rel}"
	else
		rm -f "${REF_CASE_DIR}/${rel}"
	fi
}

case_stream() {
	case_name=$1
	plist_mode=$2
	stdin_rel=$3
	shift 3

	new_case "${case_name}" "${plist_mode}"
	stdin_abs=
	if [ -n "${stdin_rel}" ]; then
		stdin_abs="${RUN_DIR}/${stdin_rel}"
	fi

	if [ "${GENERATE_REFERENCE}" = "1" ]; then
		run_one "${SOURCE}" "${RUN_DIR}" "${RUN_DIR}/run" "${stdin_abs}" "$@"
		write_reference_streams
	else
		run_one "${OURS}" "${RUN_DIR}" "${RUN_DIR}/run" "${stdin_abs}" "$@"
		compare_streams
	fi

	finish_case "${case_name}"
}

case_stream_and_exact_files() {
	case_name=$1
	plist_mode=$2
	stdin_rel=$3
	filespec=$4
	shift 4

	new_case "${case_name}" "${plist_mode}"
	stdin_abs=
	if [ -n "${stdin_rel}" ]; then
		stdin_abs="${RUN_DIR}/${stdin_rel}"
	fi

	if [ "${GENERATE_REFERENCE}" = "1" ]; then
		run_one "${SOURCE}" "${RUN_DIR}" "${RUN_DIR}/run" "${stdin_abs}" "$@"
		write_reference_streams
		for rel in ${filespec}; do
			write_reference_file "${rel}"
		done
	else
		run_one "${OURS}" "${RUN_DIR}" "${RUN_DIR}/run" "${stdin_abs}" "$@"
		compare_streams
		for rel in ${filespec}; do
			compare_file_exact "${rel}"
		done
	fi

	finish_case "${case_name}"
}

case_stream help-short missing '' -h
case_stream help-long missing '' --help
case_stream no-args missing ''
case_stream invalid-double-path missing '' one.plist two.plist
case_stream missing-c-arg missing '' -c
case_stream print-root-missing missing '' missing.plist -c 'Print'
case_stream print-missing-key missing '' missing.plist -c 'Print :missing'
case_stream_and_exact_files clear-default missing '' 'missing.plist' missing.plist -c 'Clear' -c 'Print'
case_stream_and_exact_files clear-array missing '' 'missing.plist' missing.plist -c 'Clear array' -c 'Add :0 string x' -c 'Print'
case_stream_and_exact_files add-string-spaces missing '' 'missing.plist' missing.plist -c 'Add :msg string "hello world"' -c 'Print :msg'
case_stream_and_exact_files add-string-single-quotes missing '' 'missing.plist' missing.plist -c "Add :msg string 'hello world'" -c 'Print :msg'
case_stream_and_exact_files escaped-colon-key missing '' 'missing.plist' missing.plist -c 'Add :foo\:bar string baz' -c 'Print :foo\:bar'
case_stream_and_exact_files bool-true missing '' 'missing.plist' missing.plist -c 'Add :flag bool yes' -c 'Print :flag'
case_stream_and_exact_files bool-false missing '' 'missing.plist' missing.plist -c 'Add :flag bool no' -c 'Print :flag'
case_stream_and_exact_files real-value missing '' 'missing.plist' missing.plist -c 'Add :pi real 3.5' -c 'Print :pi'
case_stream_and_exact_files integer-value missing '' 'missing.plist' missing.plist -c 'Add :n integer 42' -c 'Print :n'
case_stream_and_exact_files date-value missing '' 'missing.plist' missing.plist -c 'Add :d date "Mon Jan 01 00:00:00 UTC 2024"' -c 'Print :d'
case_stream add-duplicate missing '' missing.plist -c 'Add :a string x' -c 'Add :a string y'
case_stream_and_exact_files set-alias missing '' 'missing.plist' missing.plist -c 'Add :a string x' -c '= :a y' -c 'Print :a'
case_stream set-missing missing '' missing.plist -c 'Set :a x'
case_stream_and_exact_files set-container missing '' 'missing.plist' missing.plist -c 'Add :d dict' -c 'Set :d x'
case_stream_and_exact_files add-alias missing '' 'missing.plist' missing.plist -c '+ :a string x' -c 'Print :a'
case_stream_and_exact_files copy-alias template-dict '' 'case.plist' case.plist -c 'cp :plain :copy' -c 'Print :copy'
case_stream copy-missing-src template-dict '' case.plist -c 'Copy :nope :dst'
case_stream copy-existing-dst template-dict '' case.plist -c 'Copy :plain :root'
case_stream_and_exact_files delete-alias-rm template-dict '' 'case.plist' case.plist -c 'rm :plain' -c 'Print'
case_stream_and_exact_files delete-alias-dash template-dict '' 'case.plist' case.plist -c '- :plain' -c 'Print'
case_stream delete-missing template-dict '' case.plist -c 'Delete :nope'
case_stream_and_exact_files delete-current-container template-dict '' 'case.plist' case.plist -c 'CD :root' -c 'Delete :root' -c 'Print'
case_stream cd-valid-relative template-dict '' case.plist -c 'CD :root' -c 'Print a'
case_stream cd-noncontainer template-dict '' case.plist -c 'CD :plain'
case_stream cd-missing template-dict '' case.plist -c 'CD :nope'
case_stream print-alias-ls template-dict '' case.plist -c 'ls :root'
case_stream_and_exact_files merge-dict-into-dict template-dict '' 'case.plist' case.plist -c 'Merge merge-dict.plist :root' -c 'Print :root'
case_stream_and_exact_files merge-recursive template-dict '' 'case.plist' case.plist -c 'Merge -r merge-dict.plist :root' -c 'Print :root'
case_stream_and_exact_files merge-array-into-array template-array '' 'case.plist' case.plist -c 'Merge merge-array.plist' -c 'Print'
case_stream merge-dict-into-array-error template-array '' case.plist -c 'Merge merge-dict.plist'
case_stream merge-array-into-dict-error template-dict '' case.plist -c 'Merge merge-array.plist'
case_stream merge-missing-file template-dict '' case.plist -c 'Merge nope.plist'
case_stream_and_exact_files import-new template-dict '' 'case.plist' case.plist -c 'Import :blob blob.bin' -c 'Print :blob'
case_stream_and_exact_files import-existing template-dict '' 'case.plist' case.plist -c 'Add :blob data xyz' -c 'Import :blob blob.bin' -c 'Print :blob'
case_stream import-container-error template-dict '' case.plist -c 'Import :root blob.bin'
case_stream import-missing-file template-dict '' case.plist -c 'Import :blob nope.bin'
case_stream_and_exact_files save-then-revert missing '' 'missing.plist' missing.plist -c 'Add :a string x' -c 'Save' -c 'Set :a y' -c 'Revert' -c 'Print :a'
case_stream quit-alias template-dict stdin-quit.txt case.plist
case_stream bye-alias template-dict stdin-bye.txt case.plist
case_stream question-help-interactive template-dict stdin-help.txt case.plist
case_stream interactive-print template-dict stdin-print.txt case.plist
case_stream interactive-eof template-dict stdin-empty.txt case.plist
case_stream xml-print template-dict '' case.plist -x -c 'Print :root'
case_stream parse-error-quotes missing '' missing.plist -c 'Add :a string "unterminated'
case_stream type-error missing '' missing.plist -c 'Add :a bogus x'
case_stream_and_exact_files remove-alias-word template-dict '' 'case.plist' case.plist -c 'Remove :plain' -c 'Print'

printf 'SUMMARY pass=%s fail=%s root=%s\n' "${PASS}" "${FAIL}" "${ROOT}"
if [ "${FAIL}" -ne 0 ]; then
	sed -n '1,200p' "${FAILED_CASES}"
	exit 1
fi
