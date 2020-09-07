#! /usr/bin/env python3

'''
Creates C code for creating .docx files using internal template .docx content.

Args:

    -i <docx-path>
        Set template .docx file to extract from.

    -o <out-path>
        Set name of output files.
        
        We write to <out-path>.c and <out-path>.h.
'''

import io
import os
import sys
import textwrap


def system(command):
    e = os.system(command)
    if e:
        print(f'command failed: {command}')
        assert 0

def read(path):
    with open(path) as f:
        return f.read()

def write(text, path):
    parent = os.path.dirname(path)
    if parent:
        os.makedirs(parent, exist_ok=True)
    with open(path, 'w') as f:
        f.write(text)

def path_unsafe(path):
    if '"' in path or "'" in path or '..' in path or ' ' in path:
        return True

def main():
    path_in = None
    path_out = None
    args = iter(sys.argv[1:])
    while 1:
        try: arg = next(args)
        except StopIteration: break
        if arg == '-h' or arg == '--help':
            print(__doc__)
            return
        elif arg == '-i':
            path_in = next(args)
        elif arg == '-o':
            path_out = next(args)
        else:
            assert 0
    
    if not path_in:
        raise Exception('Need to specify -i <docx-path>')
    if not path_in:
        raise Exception('Need to specify -o <out-path>')
    
    if path_unsafe(path_in):
        raise Exception(f'Cannot use path_in={path_in!r} because unsafe for shell commands.')
    if path_unsafe(path_out):
        raise Exception(f'Cannot use path_out={path_out!r} because unsafe for shell commands.')
    path_temp = f'{path_in}.dir'
    os.system(f'rm -r "{path_temp}"')
    system(f'unzip -d {path_temp} {path_in}')
    
    out_c1 = io.StringIO()
    out_c1.write(f'#include "{os.path.basename(path_out)}.h"\n')
    out_c1.write(f'\n')
    
    out_c2 = io.StringIO()
    
    out_c3 = io.StringIO()
    
    out_c3.write(f'int extract_docx_write(extract_zip_t* zip, const char* word_document_xml, int word_document_xml_length)\n')
    out_c3.write(f'{{\n')
    out_c3.write(f'    int e = -1;\n')
    
    for dirpath, dirnames, filenames in os.walk(path_temp):
        for filename in filenames:
            #name = filename[len(path_temp)+1:]
            path = os.path.join(dirpath, filename)
            name = path[ len(path_temp)+1: ]
            text = read(os.path.join(dirpath, filename))
            text = text.replace('"', '\\"')
            text = text.replace('\n', '\\n"\n                "')
            text = f'"{text}"'
            
            if name == 'word/document.xml':
                out_c2.write(f'char extract_docx_word_document_xml[] = {text};\n')
                out_c2.write(f'int  extract_docx_word_document_xml_len = sizeof(extract_docx_word_document_xml) - 1;\n')
                out_c2.write(f'\n')
                
                out_c3.write(f'    if (word_document_xml) {{\n')
                out_c3.write(f'        if (extract_zip_write_file(zip, word_document_xml, word_document_xml_length, "{name}")) goto end;\n')
                out_c3.write(f'    }}\n')
                out_c3.write(f'    else {{\n')
            
            else:
                out_c3.write(f'    {{\n')
            
            out_c3.write(f'        char text[] = {text}\n')
            out_c3.write(f'                ;\n')
            out_c3.write(f'        if (extract_zip_write_file(zip, text, sizeof(text)-1, "{name}")) goto end;\n')
            out_c3.write(f'    }}\n')
            out_c3.write(f'    \n')
    
    out_c3.write(f'    e = 0;\n')
    out_c3.write(f'    end:\n')
    out_c3.write(f'    return e;\n')
    out_c3.write(f'}}\n')
    
    out_c = ''
    out_c += out_c1.getvalue()
    out_c += out_c2.getvalue()
    out_c += out_c3.getvalue()
    write(out_c, f'{path_out}.c')
    
    out = io.StringIO()
    out.write(f'#ifndef EXTRACT_DOCX_TEMPLATE_H\n')
    out.write(f'#define EXTRACT_DOCX_TEMPLATE_H\n')
    out.write(f'\n')
    out.write(f'#include "../zip.h"\n')
    out.write(f'\n')
    out.write(f'extern char extract_docx_word_document_xml[];\n')
    out.write(f'extern int  extract_docx_word_document_xml_length;\n')
    out.write(f'/* Contents of internal .docx template\'s word/document.xml. */\n')
    out.write(f'\n')
    out.write(f'int extract_docx_write(extract_zip_t* zip, const char* word_document_xml, int word_document_xml_length);\n')
    out.write(f'/* Writes internal template .docx items to <zip>, inserting <word_document_xml> into word/document.xml. */\n')
    out.write(f'\n')
    out.write(f'#endif\n')
    write(out.getvalue(), f'{path_out}.h')
    
if __name__ == '__main__':
    main()
