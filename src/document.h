#ifndef ARTIFEX_EXTRACT_DOCUMENT_H
#define ARTIFEX_EXTRACT_DOCUMENT_H

#include "../include/extract.h"

#ifdef _MSC_VER
    #include "compat_stdint.h"
#else
    #include <stdint.h>
#endif
#include <assert.h>

typedef struct span_t span_t;
typedef struct line_t line_t;
typedef struct paragraph_t paragraph_t;
typedef struct image_t image_t;

static const double pi = 3.141592653589793;

/*
All content is stored as content_t nodes in a doubly linked-list.
The first node in the list is a 'content_root' node. The last
node in the list is the same node again.

Thus:
  Every node in a list (including the root) has next and prev != NULL.
  The root node in an empty list has next and prev pointing to itself.
  Any non-root node with prev and next == NULL is not in a list.
*/
typedef enum {
    content_root,
    content_span,
    content_line,
    content_paragraph,
    content_image,
} content_type_t;

typedef struct content_t {
    content_type_t type;
    struct content_t *prev;
    struct content_t *next;
} content_t;


/* Initialise a content_t (just the base struct). */
void content_init(content_t *content, content_type_t type);

/* Unlink a (non-root) content_t from any list. */
void content_unlink(content_t *content);

/* Free all the content, from a (root) content_t. */
void content_clear(extract_alloc_t* alloc, content_t *root);

/* Unlink a span_t from any list. */
void content_unlink_span(span_t *span);

span_t *content_first_span(const content_t *root);
span_t *content_last_span(const content_t *root);
line_t *content_first_line(const content_t *root);
line_t *content_last_line(const content_t *root);
paragraph_t *content_first_paragraph(const content_t *root);
paragraph_t *content_last_paragraph(const content_t *root);

int content_count(content_t *root);
int content_count_images(content_t *root);
int content_count_spans(content_t *root);
int content_count_lines(content_t *root);
int content_count_paragraphs(content_t *root);

int content_new_root(extract_alloc_t *alloc, content_t **pcontent);
int content_new_span(extract_alloc_t *alloc, span_t **pspan);
int content_new_line(extract_alloc_t *alloc, line_t **pline);
int content_new_paragraph(extract_alloc_t *alloc, paragraph_t **pparagraph);

int content_append_new_span(extract_alloc_t* alloc, content_t *root, span_t **pspan);
int content_append_new_line(extract_alloc_t* alloc, content_t *root, line_t **pline);
int content_append_new_paragraph(extract_alloc_t* alloc, content_t *root, paragraph_t **pparagraph);
int content_append_new_image(extract_alloc_t* alloc, content_t *root, image_t **pimage);

void content_append(content_t *root, content_t *content);
void content_append_span(content_t *root, span_t *span);
void content_append_line(content_t *root, line_t *line);
void content_append_paragraph(content_t *root, paragraph_t *paragraph);

void content_concat(content_t *dst, content_t *src);

void content_dump(const content_t *content);

typedef int (content_cmp_fn)(const content_t *, const content_t *);

void content_sort(content_t *content, content_cmp_fn *cmp);

/* To iterate over the line elements of a content list:

content_line_iterator it;
line_t *line;

for(line = content_line_iterator_line_init(&it, content); line != NULL; line = content_line_iterator_next(&it))
{
}

*/

typedef struct {
    content_t *root;
    content_t *next;
} content_paragraph_iterator;

static inline paragraph_t *content_paragraph_iterator_next(content_paragraph_iterator *it)
{
    content_t *next;

    do {
        next = it->next;
        if (next == it->root)
            return NULL;
        assert(next->type != content_root);
        it->next = next->next;
    } while (next->type != content_paragraph);

    return (paragraph_t *)next;
}

static inline paragraph_t *content_paragraph_iterator_init(content_paragraph_iterator *it, content_t *root)
{
    it->root = root;
    it->next = root->next;

    return content_paragraph_iterator_next(it);
}

typedef struct {
    content_t *root;
    content_t *next;
} content_line_iterator;

static inline line_t *content_line_iterator_next(content_line_iterator *it)
{
    content_t *next;

    do {
        next = it->next;
        if (next == it->root)
            return NULL;
        assert(next->type != content_root);
        it->next = next->next;
    } while (next->type != content_line);

    return (line_t *)next;
}

static inline line_t *content_line_iterator_init(content_line_iterator *it, content_t *root)
{
    it->root = root;
    it->next = root->next;

    return content_line_iterator_next(it);
}

typedef struct {
    content_t *root;
    content_t *next;
} content_span_iterator;

static inline span_t *content_span_iterator_next(content_span_iterator *it)
{
    content_t *next;

    do {
        next = it->next;
        if (next == it->root)
            return NULL;
        assert(next->type != content_root);
        it->next = next->next;
    } while (next->type != content_span);

    return (span_t *)next;
}

static inline span_t *content_span_iterator_init(content_span_iterator *it, content_t *root)
{
    it->root = root;
    it->next = root->next;

    return content_span_iterator_next(it);
}

typedef struct {
    content_t *root;
    content_t *next;
} content_image_iterator;

static inline image_t *content_image_iterator_next(content_image_iterator *it)
{
    content_t *next;

    do {
        next = it->next;
        if (next == it->root)
            return NULL;
        assert(next->type != content_root);
        it->next = next->next;
    } while (next->type != content_image);

    return (image_t *)next;
}

static inline image_t *content_image_iterator_init(content_image_iterator *it, content_t *root)
{
    it->root = root;
    it->next = root->next;

    return content_image_iterator_next(it);
}

typedef struct {
    content_t *root;
    content_t *next;
} content_iterator;

static inline content_t *content_iterator_next(content_iterator *it)
{
    content_t *next = it->next;

    if (next == it->root)
        return NULL;
    assert(next->type != content_root);
    it->next = next->next;

    return next;
}

static inline content_t *content_iterator_init(content_iterator *it, content_t *root)
{
    it->root = root;
    it->next = root->next;

    return content_iterator_next(it);
}

typedef struct
{
    double x;
    double y;
} point_t;

const char* extract_point_string(const point_t* point);

typedef struct
{
    point_t min;
    point_t max;
} rect_t;

extern const rect_t extract_rect_infinite;
extern const rect_t extract_rect_empty;

rect_t extract_rect_intersect(rect_t a, rect_t b);

rect_t extract_rect_union(rect_t a, rect_t b);

int extract_rect_contains_rect(rect_t a, rect_t b);

int extract_rect_valid(rect_t a);

const char* extract_rect_string(const rect_t* rect);

typedef struct
{
    double  a;
    double  b;
    double  c;
    double  d;
    double  e;
    double  f;
} matrix_t;

const char* extract_matrix_string(const matrix_t* matrix);

double      extract_matrix_expansion(matrix_t m);
/* Returns a*d - b*c. */

point_t     extract_multiply_matrix_point(matrix_t m, point_t p);
matrix_t    extract_multiply_matrix_matrix(matrix_t m1, matrix_t m2);

int extract_matrix_cmp4(const matrix_t* lhs, const matrix_t* rhs)
;
/* Returns zero if first four members of *lhs and *rhs are equal, otherwise
+/-1. */

typedef struct
{
    /* (x,y) before transformation by ctm and trm. */
    double      pre_x;
    double      pre_y;

    /* (x,y) after transformation by ctm and trm. */
    double      x;
    double      y;

    unsigned    ucs;
    double      adv;

    rect_t bbox;
} char_t;
/* A single char in a span.
*/

struct span_t
{
    content_t   base;
    matrix_t    ctm;
    matrix_t    trm;
    char*       font_name;

    /* font size is extract_matrix_cmp4(trm). */

    struct {
        unsigned font_bold      : 1;
        unsigned font_italic    : 1;
        unsigned wmode          : 1;
    } flags;

    char_t*     chars;
    int         chars_num;
};
/* List of chars that have same font and are usually adjacent. */

void extract_span_init(span_t* span);

void extract_span_free(extract_alloc_t* alloc, span_t** pspan);
/* Frees a span_t, returning with *pspan set to NULL. */

char_t* extract_span_char_last(span_t* span);
/* Returns last character in span. */

int extract_span_append_c(extract_alloc_t* alloc, span_t* span, int c);
/* Appends new char_t to an span_t with .ucs=c and all other
fields zeroed. */

const char* extract_span_string(extract_alloc_t* alloc, span_t* span);
/* Returns static string containing info about span_t. */

struct line_t
{
    content_t base;
    content_t content;
};
/* List of spans that are aligned on same line. */

void extract_line_init(line_t *line);

void extract_line_free(extract_alloc_t* alloc, line_t** pline);

span_t* extract_line_span_first(line_t* line);
/* Returns first span in a line. */

span_t* extract_line_span_last(line_t* line);
/* Returns last span in a line. */

struct paragraph_t
{
    content_t base;
    content_t content;
};
/* List of lines that are aligned and adjacent to each other so as to form a
paragraph. */

void extract_paragraph_init(paragraph_t *paragraph);

void extract_paragraph_free(extract_alloc_t* alloc, paragraph_t** pparagraph);

int extract_paragraph_alloc(extract_alloc_t* alloc, paragraph_t** pparagraph);

struct image_t
{
    content_t base;
    char*   type;   /* jpg, png etc. */
    char*   name;   /* Name of image file within docx. */
    char*   id;     /* ID of image within docx. */
    double  x;
    double  y;
    double  w;
    double  h;
    void*   data;
    size_t  data_size;

    extract_image_data_free data_free;
    void*                   data_free_handle;

};
/* Information about an image. <type> is as passed to extract_add_image();
<name> and <id> are created to be unique identifiers for use in generated docx
file. */

void extract_image_init(image_t* image);

void extract_image_clear(extract_alloc_t* alloc, image_t* image);

void extract_image_free(extract_alloc_t *alloc, image_t **pimage);

typedef struct
{
    float   color;
    rect_t  rect;
} tableline_t;
/* A line that is part of a table. */

typedef struct
{
    tableline_t*    tablelines;
    int             tablelines_num;
} tablelines_t;


typedef struct
{
    rect_t          rect;

    /* If left/above is true, this cell is not obscured by cell to its
    left/above. */
    uint8_t         left;
    uint8_t         above;

    /* extend_right and extend_down are 1 for normal cells, 2 for cells which
    extend right/down to cover an additional column/row, 3 to cover two
    additional columns/rows etc. */
    int             extend_right;
    int             extend_down;

    /* Contents of this cell. */
    content_t       lines;
    content_t       paragraphs;
} cell_t;
/* A cell within a table. */

void extract_cell_init(cell_t* cell);
void extract_cell_free(extract_alloc_t* alloc, cell_t** pcell);

typedef struct
{
    point_t     pos;    /* top-left. */

    /* Array of cells_num_x*cells_num_y cells; cell (x, y) is:
        cells_num_x * y + x.
    */
    cell_t**    cells;
    int         cells_num_x;
    int         cells_num_y;
} table_t;


typedef enum
{
    SPLIT_NONE = 0,
    SPLIT_HORIZONTAL,
    SPLIT_VERTICAL
} split_type_t;


typedef struct split_t
{
    split_type_t type;
    double weight;
    int count;
    struct split_t *split[1];
} split_t;

typedef struct
{
    rect_t      mediabox;

    int         images_num;
    content_t   content;

    content_t   lines;
    /* These refer to items in .spans. Initially empty, then set by
    extract_join(). */

    content_t   paragraphs;
    /* These refer to items in .lines. Initially empty, then set
    by extract_join(). */

    tablelines_t    tablelines_horizontal;
    tablelines_t    tablelines_vertical;

    table_t**   tables;
    int         tables_num;
} subpage_t;
/* A subpage. Contains different representations of the list of spans. */


typedef struct
{
    rect_t      mediabox;

    subpage_t** subpages;
    int         subpages_num;

    split_t*    split;
} extract_page_t;
/* A page. Contains a list of subpages. NB not
called page_t because this clashes with a system type on hpux. */


typedef struct
{
    extract_page_t**    pages;
    int                 pages_num;
} document_t;
/* A list of pages. */


typedef struct
{
    image_t**   images;
    int         images_num;
    char**      imagetypes;
    int         imagetypes_num;
} images_t;


int extract_document_join(extract_alloc_t* alloc, document_t* document, int layout_analysis);
/* This does all the work of finding paragraphs and tables. */

double extract_matrices_to_font_size(matrix_t* ctm, matrix_t* trm);

/* Things below here are used when generating output. */

typedef struct
{
    char*   name;
    double  size;
    int     bold;
    int     italic;
} font_t;
/* Basic information about current font. */

typedef struct
{
    font_t      font;
    matrix_t*   ctm_prev;
} content_state_t;
/* Used to keep track of font information when writing paragraphs of odt
content, e.g. so we know whether a font has changed so need to start a new odt
span. */

int extract_page_analyse(extract_alloc_t* alloc, extract_page_t* page);
/* Analyse page content for layouts. */

int extract_subpage_alloc(extract_alloc_t* extract, rect_t mediabox, extract_page_t* page, subpage_t** psubpage);
/* content_t constructor. */

void extract_subpage_free(extract_alloc_t* alloc, subpage_t** psubpage);
/* subpage_t destructor. */

int extract_split_alloc(extract_alloc_t* alloc, split_type_t type, int count, split_t** psplit);
/* Allocate a split_t. */

void extract_split_free(extract_alloc_t* alloc, split_t** psplit);

/* Some helper functions */

/* Return a span_t * pointer to the first element in a content list. */
static inline span_t *content_head_as_span(content_t *root)
{
    assert(root != NULL && root->type == content_root && (root->next == NULL || root->next->type == content_span));
    return (span_t *)root->next;
}

#endif
