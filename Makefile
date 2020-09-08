# Example commands:
#   make
#   make test
#   make build=debug-opt
#   make build=opt test
#


# Build flags.
#
build = debug

flags_link      = -W -Wall -lm
flags_compile   = -W -Wall -Wmissing-declarations -Wmissing-prototypes -Werror -MMD -MP -I include

# We assume that mutool and gs are available at hard-coded paths.
#
gs      = ../ghostpdl/debug-bin/gs
mutool  = ../mupdf/build/debug/mutool

ifeq ($(build),)
    $(error Need to specify build=debug|opt|debug-opt|memento)
else ifeq ($(build),debug)
    flags_link      += -g
    flags_compile   += -g
else ifeq ($(build),opt)
    flags_link      += -O2
    flags_compile   += -O2
else ifeq ($(build),debug-opt)
    flags_link      += -g -O2
    flags_compile   += -g -O2
else ifeq ($(build),memento)
    flags_link      += -g
    flags_compile   += -g -D MEMENTO
else
    $(error unrecognised $$(build)=$(build))
endif


# Source code.
#
exe_src = src/extract-exe.c src/extract.c src/astring.c src/docx.c src/outf.c src/xml.c src/zip.c

ifeq ($(build),memento)
    exe_src += src/memento.c
endif


# Build files.
#
exe = src/build/extract-$(build).exe
exe_obj = $(patsubst src/%.c, src/build/%.c-$(build).o, $(exe_src)) src/build/docx_template.c-$(build).o
exe_dep = $(exe_obj:.o=.d)


# Test targets and rules.
#

test_files = test/Python2.pdf test/zlib.3.pdf

test_targets_mu             = $(patsubst test/%, test/generated/%.mu.intermediate.xml.content.xml.diff, $(test_files))
test_targets_mu_autosplit   = $(patsubst test/%, test/generated/%.mu.intermediate.xml.autosplit.content.xml.diff, $(test_files))
test_targets_gs             = $(patsubst test/%, test/generated/%.gs.intermediate.xml.content.xml.diff, $(test_files))

test: $(test_targets_mu) $(test_targets_mu_autosplit) $(test_targets_gs)

test/generated/%.pdf.mu.intermediate.xml: test/%.pdf
	@echo
	@echo Generating intermediate file for $< with mutool.
	@mkdir -p test/generated
	$(mutool) draw -F xmltext -o $@ $<

test/generated/%.pdf.gs.intermediate.xml: test/%.pdf
	@echo
	@echo Generating intermediate file for $< with gs.
	@mkdir -p test/generated
	$(gs) -sDEVICE=txtwrite -dTextFormat=4 -o $@ $<

test/generated/%.intermediate.xml.content.xml: test/generated/%.intermediate.xml $(exe)
	@echo
	@echo Generating content with extract.exe
	./$(exe) -i $< --o-content $@ -o $<.docx

test/generated/%.intermediate.xml.autosplit.content.xml: test/generated/%.intermediate.xml $(exe)
	@echo
	@echo Generating content with extract.exe autosplit
	./$(exe) -i $< --autosplit 1 --o-content $@ -o $<.docx

test/generated/%.intermediate.xml.content.xml.diff: test/generated/%.intermediate.xml.content.xml test/%.content.ref.xml
	@echo Diffing content with reference output.
	diff -u $^ >$@
test/generated/%.intermediate.xml.autosplit.content.xml.diff: test/generated/%.intermediate.xml.autosplit.content.xml test/%.content.ref.xml
	diff -u $^ >$@


# Build rules.
#

# Convenience target to build main executable.
#
exe: $(exe)

# Rule for main executble.
#
$(exe): $(exe_obj)
	cc $(flags_link) -o $@ $^ -lz

# Compile rule. We always include src/build/docx_template.c as a prerequisite
# in case code #includes docx_template.h.
#
src/build/%.c-$(build).o: src/%.c src/build/docx_template.c
	@mkdir -p src/build
	cc -c $(flags_compile) -o $@ $<

src/build/%.c-$(build).o: src/build/%.c
	@mkdir -p src/build
	cc -c $(flags_compile) -o $@ $<


# Clean rules.
#
.PHONY: clean
clean:
	-rm -r src/build test/generated


# Rule for build/docx_template.c.
#
src/build/docx_template.c: .ALWAYS
	@echo Building $@
	@mkdir -p src/build
	./src/docx_template_build.py -i src/template.docx -o src/build/docx_template
.ALWAYS:

# Copy generated files to website.
#
web:
	rsync -ai test/*.docx *.pdf julian@casper.ghostscript.com:public_html/extract/

# Dynamic dependencies.
#
-include $(exe_dep)
