#!/usr/bin/env python3

import glob
import subprocess
import sys
import os

dir_mupdf = os.path.relpath(f'{__file__}/../../../..')
dir_pdfs = os.path.relpath(f'{__file__}/../../../../../camelot/docs/benchmark/lattice')
print(f'dir_mupd={dir_mupdf}')
print(f'dir_pdfs={dir_pdfs}')


class Pdf:
    def __init__(self, path):
        self.pdf = path
        self.leaf = os.path.basename(path)
        self.out_html = f'test/generated/{self.leaf}.mutool.html'
        csv_refs_glob = f'{self.pdf[:-4]}-data-camelot-page-*-table-*.csv'
        self.csv_refs = glob.glob(csv_refs_glob)
        self.out_csv_arg = f'test/generated/{self.leaf}.mutool-%i.csv'
        
    def csv_path(self, i):
        return f'test/generated/{self.leaf}.mutool-{i}.csv'
    
    def csvs(self):
        for i, csv_ref in enumerate(self.csv_refs):
            csv_generated = self.csv_path(i)
            yield csv_ref, csv_generated

def get_pdfs():
    pdfs_globs = f'{dir_pdfs}/*/*.pdf'
    print(f'pdfs_globs={pdfs_globs}')
    pdfs = glob.glob(pdfs_globs)
    for pdf in pdfs:
        yield Pdf(pdf)

def run_tests():
    mutool = f'{dir_mupdf}/build/debug-extract/mutool'

    for pdf in get_pdfs():
        command = f'{mutool} convert -F docx -O html,tables-csv-format={pdf.out_csv_arg} -o {pdf.out_html} {pdf.pdf}'
        print(f'Running: {command}')
        sys.stdout.flush()
        subprocess.check_call(command, shell=1)
        print(f'Have converted {pdf} to:')
        print(f'    {pdf.out_html}')
        print(f'    {pdf.out_csv_arg}')
        for csv_ref, csv_generated in pdf.csvs():
            command_diff = f'diff -uw {csv_ref} {csv_generated}'
            print(f'Running command: {command_diff}')
            sys.stdout.flush()
            subprocess.check_call(command_diff, shell=1)

if __name__ == '__main__':
    args = iter(sys.argv[1:])
    while 1:
        try:
            arg = next(args)
        except Exception:
            break
        if arg == 'test':
            run_tests()
        elif arg == 'upload':
            destination = next(args)
            
            command = f'rsync -ai \\\n'
            for pdf in get_pdfs():
                command += f' {pdf.pdf} {pdf.out_html} \\\n'
                for csv_ref, csv_generated in pdf.csvs():
                    command += f' {csv_ref} \\\n'
                for csv_ref, csv_generated in pdf.csvs():
                    command += f' {csv_generated} \\\n'
            command += f' {destination}'
            print(f'Running: {command}')
            subprocess.check_call(command, shell=1)
