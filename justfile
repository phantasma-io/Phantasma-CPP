[private]
default:
    @just --list

[group('test')]
test:
    make -C tests clean
    make -C tests run

alias t := test

[group('refactoring')]
check-eols:
    find . -not -type d -name "*.cpp" -exec file "{}" ";" | grep CRLF

[group('refactoring')]
format:
    bash -lc 'while IFS= read -r -d "" file; do case "$file" in *.c|*.cc|*.cpp|*.cxx|*.h|*.hpp) clang-format -i --style=file "$file" ;; esac; dos2unix -q "$file"; done < <(git ls-files -z -- include tests "samples/Basic Examples/LowLevelSample/main.cpp" "samples/Basic Examples/CurlRapidjsonSample/main.cpp" "samples/Basic Examples/CpprestSample/main.cpp" samples/WalletSample/program.cpp)'

alias f := format

[group('refactoring')]
fix-eols:
    bash -lc 'while IFS= read -r -d "" file; do dos2unix -q "$file"; done < <(git ls-files -z -- include tests "samples/Basic Examples/LowLevelSample/main.cpp" "samples/Basic Examples/CurlRapidjsonSample/main.cpp" "samples/Basic Examples/CpprestSample/main.cpp" samples/WalletSample/program.cpp)'
