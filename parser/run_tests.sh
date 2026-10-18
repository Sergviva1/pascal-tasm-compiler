#!/usr/bin/env bash
# Run from any directory using Bash / Git Bash.
set -eu
cd "$(dirname "$0")/.."
root=$PWD
cxx=${CXX:-g++}
# In Git Bash, load DLLs belonging to the selected MinGW compiler first.
compiler=$(command -v "$cxx")
export PATH="$(dirname "$compiler"):$PATH"
mkdir -p build
for target in main declaration_tests parser_tests; do
    output=$target
    if [ "$target" = main ]; then output=parser; fi
    "$cxx" -std=c++17 -O2 -Wall -Wextra -Wpedantic -I. "parser/$target.cpp" -o "build/$output.exe"
done
./build/declaration_tests.exe
./build/parser_tests.exe

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
cd "$tmp"
"$root/build/parser.exe" "$root/tests/tokens.lst"
# Compare actual CLI output to fixtures structurally (symbol order is irrelevant).
(cd "$root" && ./build/parser_tests.exe "$tmp")
rm tree.json symbols.tab

reject_file() {
    name=$1
    data=$2
    printf '%s' "$data" > input.json
    code=0
    "$root/build/parser.exe" input.json > stdout.txt 2> stderr.txt || code=$?
    if [ "$code" -ne 1 ] || [ -e tree.json ] || [ -e symbols.tab ]; then
        printf 'FAIL: %s (exit %s or unexpected output files)\n' "$name" "$code"
        exit 1
    fi
    grep '^parser:' stderr.txt > /dev/null
    printf 'PASS: CLI %s\n' "$name"
}
reject_file 'malformed JSON' '[{'
reject_file 'trailing JSON data' '[] {}'
reject_file 'non-array root' '{}'
reject_file 'empty stream' '[]'
reject_file 'non-object token' '[42]'
reject_file 'missing type' '[{"value":"","line":1}]'
reject_file 'non-string value' '[{"type":"EOF","value":0,"line":1}]'
reject_file 'missing line' '[{"type":"EOF","value":""}]'
reject_file 'fractional line' '[{"type":"EOF","value":"","line":1.5}]'
reject_file 'zero line' '[{"type":"EOF","value":"","line":0}]'
reject_file 'negative line' '[{"type":"EOF","value":"","line":-1}]'
reject_file 'line overflow' '[{"type":"EOF","value":"","line":18446744073709551615}]'
reject_file 'decreasing lines' '[{"type":"IDENT","value":"x","line":2},{"type":"EOF","value":"","line":1}]'
reject_file 'unknown type' '[{"type":"NUMBER","value":"1","line":1}]'
reject_file 'missing EOF' '[{"type":"IDENT","value":"x","line":1}]'
reject_file 'early EOF' '[{"type":"EOF","value":"","line":1},{"type":"IDENT","value":"x","line":1}]'
reject_file 'duplicate EOF' '[{"type":"EOF","value":"","line":1},{"type":"EOF","value":"","line":1}]'
reject_file 'nonempty EOF' '[{"type":"EOF","value":"end","line":1}]'
reject_file 'syntax error' '[{"type":"EOF","value":"","line":1}]'
reject_file 'semantic error' '[{"type":"KEYWORD","value":"program","line":1},{"type":"IDENT","value":"P","line":1},{"type":"DELIM","value":";","line":1},{"type":"KEYWORD","value":"begin","line":2},{"type":"IDENT","value":"x","line":3},{"type":"OP","value":":=","line":3},{"type":"INT_LITERAL","value":"1","line":3},{"type":"KEYWORD","value":"end","line":4},{"type":"DELIM","value":".","line":4},{"type":"EOF","value":"","line":4}]'
grep 'parser: line 3: undeclared' stderr.txt > /dev/null

code=0
"$root/build/parser.exe" > stdout.txt 2> stderr.txt || code=$?
test "$code" -eq 1
grep '^Usage:' stderr.txt > /dev/null
code=0
"$root/build/parser.exe" missing.json > stdout.txt 2> stderr.txt || code=$?
test "$code" -eq 1
grep '^parser:' stderr.txt > /dev/null
test ! -e tree.json && test ! -e symbols.tab
# A failed parse must not overwrite successful output from an earlier run.
printf 'tree sentinel' > tree.json
printf 'symbols sentinel' > symbols.tab
code=0
"$root/build/parser.exe" input.json > stdout.txt 2> stderr.txt || code=$?
test "$code" -eq 1
test "$(<tree.json)" = 'tree sentinel'
test "$(<symbols.tab)" = 'symbols sentinel'
printf 'All parser CLI checks passed\n'
