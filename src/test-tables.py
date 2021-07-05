#!/usr/bin/env python3

import glob
import subprocess
import sys
import os

dir_root = os.path.abspath(f'{__file__}/../..')
dir_mupdf = os.path.abspath(f'{dir_root}/../..')
pdfs_globs = os.path.abspath(f'{dir_mupdf}/../camelot/docs/benchmark/lattice/*/*.pdf')
print(f'pdfs_globs={pdfs_globs}')
pdfs = glob.glob(pdfs_globs)

mutool = f'{dir_mupdf}/build/debug-extract/mutool'

for pdf in pdfs:
    leaf = os.path.basename(pdf)
    out_html = f'test/generated/{leaf}.mutool.html'
    out_csv = f'test/generated/{leaf}.mutool-%i.csv'
    command = f'{mutool} convert -F docx -O html,tables-csv-format={out_csv} -o {out_html} {pdf}'
    print(f'Running: {command}')
    subprocess.check_call(command, shell=1)
    print(f'Have converted {pdf} to:')
    print(f'    {out_html}')
    print(f'    {out_csv}')
    csv_refs_glob = f'{pdf[:-4]}-data-camelot-page-*-table-*.csv'
    csv_refs = glob.glob(csv_refs_glob)
    print(f'csv_refs_glob={csv_refs_glob}')
    print(f'csv_refs={csv_refs}')
    for i, csv_ref in enumerate(csv_refs):
        csv_generated = f'test/generated/{leaf}.mutool-{i}.csv'
        command_diff = f'diff -uw {csv_ref} {csv_generated}'
        print(f'Running command: {command_diff}')
        subprocess.check_call(command_diff, shell=1)
