[private]
default:
    @just --list

[group('test')]
test:
    make -C tests clean
    make -C tests run

alias t := test
