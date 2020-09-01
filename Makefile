# Example commands:
#   make
#   make test
#   make build=debug-opt
#   make build=opt
#


# Build flags.
#
build = debug

flags_link      = -W -Wall -lm
flags_compile   = -W -Wall -Werror -MMD -MP

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
src = extract.c extract-exe.c

ifeq ($(build),memento)
    src += memento.c
endif


# Build files.
#
exe = build/extract-$(build).exe
obj = $(src:.c=.c-$(build).o)
obj := $(addprefix build/, $(obj))
dep = $(obj:.o=.d)


# Test rules.
#

test: test-mu test-gs test-mu-as

test-mu: Python2.pdf-test-mu zlib.3.pdf-test-mu
test-gs: zlib.3.pdf-test-gs Python2.pdf-test-gs
test-mu-as: Python2.pdf-test-mu-as zlib.3.pdf-test-mu-as

%.pdf-test: %.pdf-test-mu %.pdf-test-gs

# Test using mutool to generate intermediate.
#
%.pdf-test-mu: %.pdf $(exe)
	@echo
	@echo === Testing $< with mutool.
	mkdir -p test
	@echo == Generating intermediate with mutool.
	$(mutool) draw -F xmltext -o test/$<.mu.intermediate.xml $<
	@echo == Generating output.
	./$(exe) -i test/$<.mu.intermediate.xml --o-content test/$<.mu.content.xml -o test/$<.mu.docx -p 1 -t template.docx
	@echo == Comparing output with reference output.
	diff -u test/$<.mu.content.xml $<.mu.content.ref.xml
	@echo == Test succeeded.

# Run extract.exe with --autosplit, to stress joining of spans. Compare with
# the default .ref file.
%.pdf-test-mu-as: %.pdf $(exe)
	@echo
	@echo === Testing $< with mutool and autosplit.
	mkdir -p test
	@echo == Generating intermediate with mutool.
	$(mutool) draw -F xmltext -o test/$<.mu.intermediate.xml $<
	@echo == Generating output.
	./$(exe) --autosplit 1 -i test/$<.mu.intermediate.xml --o-content test/$<.mu.as.content.xml -o test/$<.mu.as.docx -p 1 -t template.docx
	@echo == Comparing output with reference output.
	diff -u test/$<.mu.as.content.xml $<.mu.content.ref.xml
	@echo == Test succeeded.

# Test using gs to generate intermediate.
#
%.pdf-test-gs: %.pdf $(exe)
	@echo
	@echo === Testing $< with gs
	mkdir -p test
	@echo == Generating intermediate with gs.
	$(gs) -sDEVICE=txtwrite -dTextFormat=4 -o test/$<.gs.intermediate.xml $<
	@echo == Generating output.
	./$(exe) -i test/$<.gs.intermediate.xml --o-content test/$<.gs.content.xml -o test/$<.gs.docx -p 1 -t template.docx
	@echo == Comparing output with reference output.
	diff -u test/$<.gs.content.xml $<.gs.content.ref.xml
	@echo == Test succeeded.


# Build rules.
#
$(exe): $(obj)
	mkdir -p build
	cc $(flags_link) -o $@ $^

build/%.c-$(build).o: %.c
	mkdir -p build
	cc -c $(flags_compile) -o $@ $<


# Clean rule.
#
.PHONY: clean
clean:
	rm $(obj) $(dep) $(exe)

clean-all:
	rm -r build test 


# Copy generated files to website.
#
web:
	rsync -ai test/*.docx *.pdf julian@casper.ghostscript.com:public_html/extract/

# Dynamic dependencies.
#
-include $(dep)
