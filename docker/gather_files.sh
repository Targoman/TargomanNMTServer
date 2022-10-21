#!/bin/bash
cd $(dirname "$0")
mkdir -p files_1.9.0_cpu
rm -f files_1.9.0_cpu/*
cp -f $(ldd ../out/bin/TargomanNMTServer | perl -pe 's%(?:\s*(\S+)\s+=>)?\s+(\S+)\s+.*%\2%g' | fgrep -v vdso.so) files_1.9.0_cpu/
cp -f ../out/bin/TargomanNMTServer files_1.9.0_cpu/
cp -f run_nmt_server.sh files_1.9.0_cpu/
