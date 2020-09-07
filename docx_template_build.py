#! /usr/bin/env python3

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
    with open(path, 'w') as f:
        f.write(text)

if __name__ == '__main__':
    args = iter(sys.argv[1:])
    while 1:
        try: arg = next(args)
        except StopIteration: break
        if arg == '-i':
            path_in = next(args)
        elif arg == '-o':
            path_out = next(args)
        else:
            assert 0
    
    path_temp = f'{path_in}.dir'
    if '"' in path_temp or '..' in path_temp or not path_temp.endswith('.dir'):
        raise Exception(f'path_temp={path_temp!r} contains double-quote')
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
    out.write(f'#include "../zip.h"\n')
    out.write(f'\n')
    out.write(f'extern char extract_docx_word_document_xml[];\n')
    out.write(f'extern int  extract_docx_word_document_xml_length;\n')
    out.write(f'\n')
    out.write(f'int extract_docx_write(extract_zip_t* zip, const char* word_document_xml, int word_document_xml_length);\n')
    out.write(f'/* Writes template files to <zip>; word_document_xml overrides content for word/document.xml. */\n')
    write(out.getvalue(), f'{path_out}.h')
    
