#!/usr/bin/env python3

import glob
import subprocess
import sys
import time
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
        if self.leaf == 'background_lines_1.pdf':
            self.csv_refs = self.csv_refs[1:]
        
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

def make_textfile(path_in, path_out):
    print(f'Converting {path_in} to {path_out}')
    try:
        f_in0 = open(path_in, 'rb')
    except Exception:
        return
    with f_in0 as f_in:
        with open(path_out, 'w') as f_out:
            for line in f_in:
                #print(f'line={line}')
                print(repr(line), file=f_out)

def run_tests():
    mutool = f'{dir_mupdf}/build/debug-extract/mutool'
    num_errors = 0
    num = 0
    summary = ''
    for pdf in get_pdfs():
        print()
        print('-'*80)
        print(f'Testing: {pdf.pdf}')
        print(f'    Output: file://{os.path.abspath(pdf.out_html)}')
        command = f'{mutool} convert -F docx -O html,tables-csv-format={pdf.out_csv_arg} -o {pdf.out_html} {pdf.pdf}'
        print(f'Running: {command}')
        sys.stdout.flush()
        subprocess.check_call(command, shell=1)
        print(f'Have converted {pdf} to:')
        print(f'    {pdf.out_html}')
        print(f'    {pdf.out_csv_arg}')
        summary += f'{pdf.pdf}:\n'
        for csv_ref, csv_generated in pdf.csvs():
            make_textfile(csv_ref, f'{csv_ref}.txt')
            make_textfile(csv_generated, f'{csv_generated}.txt')
            if pdf.leaf == 'column_span_2.pdf':
                # Special case - modify reference cvs to match our (better)
                # output - we rmove a '-' in 'Preva-lence'.
                #
                with open(f'{csv_ref}.txt') as f:
                    t = f.read()
                t2 = t
                t2 = t2.replace(
                        'b\'"Investigations","No. ofHHs","Age/Sex/Physiological  Group","Preva-lence","C.I*","RelativePrecision","Sample sizeper State"\\n\'',
                        'b\'"Investigations","No. of HHs","Age/Sex/Physiological Group","Prevalence","C.I*","Relative Precision","Sample size per State"\\n\'',
                        )
                nl = '\n'
                assert t2 != t, f'lines are: {nl.join(t.split(nl))}'
                with open(f'{csv_ref}.txt', 'w') as f:
                    f.write(t2)
            # We use 'diff -w' because camelot output seems to be missing
            # spaces where it joins lines.
            #
            command_diff = f'diff -uw {csv_ref}.txt {csv_generated}.txt'
            print(f'Running command: {command_diff}')
            sys.stdout.flush()
            if subprocess.run(command_diff, shell=1).returncode:
                print(f'Diff returned non-zero.')
                num_errors += 1
                summary += f'    e=1: {csv_generated}\n'
            else:
                print(f'Diff returned zero.')
                summary += f'    e=0: {csv_generated}\n'
            num += 1
    print(f'{summary}')
    print(f'errors/all={num_errors}/{num}')

def compare_csv(ref_csv, csv):
    make_textfile(ref_csv, f'{ref_csv}.txt')
    make_textfile(csv, f'{csv}.txt')
    command = f'diff -u {ref_csv}.txt {csv}.txt'
    subprocess.run(command_diff, shell=1)

if __name__ == '__main__':
    args = iter(sys.argv[1:])
    while 1:
        try:
            arg = next(args)
        except Exception:
            break
        if 0:
            pass
        elif arg == 'build':
            command = f'../../../julian-tools/jtest.py -b ../../build/debug-extract/ .'
            subprocess.check_call(command, shell=1)
        
        elif arg == 'test':
            run_tests()
        
        elif arg == 'html':
            path_out = 'extract-table.html'
            print(f'Creating: {path_out}')
            with open(path_out, 'w') as f:
                print(f'<html>', file=f)
                print(f'<body>', file=f)
                print(f'<h1>Extract table output ({time.strftime("%F %T")})</h1>', file=f)
                print(f'<p>&lt;source&gt; &lt;generated html&gt; &lt;reference html&gt; [&lt;generated docx&gt;]', file=f)
                print(f'<ul>', file=f)
                for path in glob.glob('test/*.pdf'):
                    leaf = os.path.basename(path)
                    if leaf in ('Python2.pdf', 'Python2clipped.pdf', 'zlib.3.pdf', 'text_graphic_image.pdf'):
                        continue
                    print(f'    <li>{leaf} ', file=f)
                    print(f'    <ul>', file=f)
                    print(f'        <li>', file=f)
                    print(f'            <a href="{path}">{path}</a>', file=f)
                    print(f'             => ', file=f)
                    print(f'             <a href="test/generated/{leaf}.mutool.html">test/generated/{leaf}.mutool.html</a>', file=f)
                    print(f'             <a href="test/{leaf}.mutool.html.ref">test/{leaf}.mutool.html.ref</a>', file=f)
                    print(f'             [<a href="test/generated/{leaf}.mutool.docx">test/generated/{leaf}.mutool.docx</a>]', file=f)
                    print(f'        <li>', file=f)
                    print(f'            <iframe width=30% height=300 src="{path}"></iframe>', file=f)
                    print(f'            <iframe width=30% height=300 src="test/generated/{leaf}.mutool.html"></iframe>', file=f)
                    #print(f'            <iframe width=25% height=300 src="test/generated/{leaf}.mutool.docx"></iframe>', file=f)
                    print(f'            <iframe width=30% height=300 src="test/{leaf}.mutool.html.ref"></iframe>', file=f)
                    print(f'    </ul>', file=f)
                print(f'</ul>', file=f)
                print(f'</body>', file=f)
                print(f'</html>', file=f)
        
        elif arg == 'upload':
            #destination = next(args)
            destination = 'julian@casper.ghostscript.com:public_html/extract/'
            command = f'rsync -aiR \\\n'
            command += f' extract-table.html'
            for path in glob.glob('test/*.pdf'):
                leaf = os.path.basename(path)
                command += f' {path} \\\n'
                command += f' test/generated/{leaf}.mutool.html \\\n'
                command += f' test/generated/{leaf}.mutool.docx \\\n'
                command += f' test/{leaf}.mutool.html.ref \\\n'
                #for csv_ref, csv_generated in pdf.csvs():
                #    command += f' {csv_ref} \\\n'
                #for csv_ref, csv_generated in pdf.csvs():
                #    command += f' {csv_generated} \\\n'
            command += f' {destination}'
            print(f'Running: {command}')
            subprocess.check_call(command, shell=1)

        else:
            raise Exception(f'Unrecognised arg: {arg!r}')
