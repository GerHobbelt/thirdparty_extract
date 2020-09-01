#include "extract.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char** argv)
{
    const char* docx_out_path       = NULL;
    const char* input_path          = NULL;
    const char* docx_template_path  = NULL;
    const char* content_path        = NULL;
    int         preserve_dir        = 0;
    int         spacing             = 1;
    int         autosplit           = 0;
    float       debugscale          = 0;

    for (int i=1; i<argc; ++i) {
        const char* arg = argv[i];
        if (!strcmp(arg, "-h") || !strcmp(arg, "--help")) {
            printf(
                    "Generates a .docx file.\n"
                    "\n"
                    "Input:\n"
                    "\n"
                    "    We require a file containing XML output from one of these:\n"
                    "        mutool draw -F xmltext ...\n"
                    "        gs -sDEVICE=txtwrite -dTextFormat=4...\n"
                    "\n"
                    "    We also requires a template .docx file\n"
                    "\n"
                    "Args:\n"
                    "    --autosplit\n"
                    "        Initially split spans when y coordinate changes. This stresses our\n"
                    "        handling of spans when input is from mupdf.\n"
                    "    -i <input-path>\n"
                    "        Name of XML file containing intermediate text spans.\n"
                    "    -o <docx-path>\n"
                    "        Output .docx file.\n"
                    "    --o-content <path>\n"
                    "        If specified, we write raw .docx content to <path>; this is the\n"
                    "        text that we embed inside the template word/document.xml file\n"
                    "        when generating the .docx.\n"
                    "    -p 0|1\n"
                    "        If 1, we preserve uncompressed <docx-path>.lib/ directory.\n"
                    "    -s 0|1\n"
                    "        If 1, we insert extra vertical space between paragraphs and extra\n"
                    "        vertical space between paragraphs that had different ctm matrices\n"
                    "        in the original document.\n"
                    "    -t <docx-template>\n"
                    "        Name of docx file to use as template.\n"
                    );
        }
        else if (!strcmp(arg, "--autosplit")) {
            autosplit = atoi(argv[++i]);
        }
        else if (!strcmp(arg, "--o-content")) {
            content_path = argv[++i];
        }
        else if (!strcmp(arg, "-i")) {
            input_path = argv[++i];
        }
        else if (!strcmp(arg, "-o")) {
            docx_out_path = argv[++i];
        }
        else if (!strcmp(arg, "-p")) {
            preserve_dir = atoi(argv[++i]);
        }
        else if (!strcmp(arg, "-s")) {
            spacing = atoi(argv[++i]);
        }
        else if (!strcmp(arg, "-t")) {
            docx_template_path = argv[++i];
        }
        else if (!strcmp(arg, "--scale")) {
            debugscale = atof(argv[++i]);
        }
        else {
            extract_outf("Unrecognised arg: '%s'", arg);
            return 1;
        }

        assert(i < argc);
    }

    assert(input_path);
    assert(docx_out_path);
    assert(docx_template_path);

    int e = -1;
    extract_string_t content;
    extract_string_init(&content);
    extract_document_t  document;
    extract_document_init(&document);

    if (extract_read_spans_raw(input_path, &document, autosplit, debugscale)) {
        extract_outf("Failed to read 'raw' output from: %s", input_path);
        goto end;
    }
    
    if (document.pages_num) {
        if (extract_document_to_docx_content(&document, &content, spacing, debugscale)) {
            extract_outf("Failed to create docx content errno=%i: %s", errno, strerror(errno));
            goto end;
        }
    }

    if (content_path) {
        extract_outf("Writing content to: %s", content_path);
        FILE* f = fopen(content_path, "w");
        assert(f);
        fwrite(content.chars, content.chars_num, 1 /*nmemb*/, f);
        fclose(f);
    }
    extract_outf("Creating .docx file: %s", docx_out_path);
    e = extract_docx_create(&content, docx_template_path, docx_out_path, preserve_dir);

    end:

    extract_string_free(&content);
    extract_document_free(&document);

    if (e) {
        extract_outf("Failed, errno: %s", strerror(errno));
    }
    else {
        extract_outf("Finished.");
    }

    return e;
}
