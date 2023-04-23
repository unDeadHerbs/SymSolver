# Author : Murray Fordyce

# README
#
# I hope this is readable for anybody who wants to learn.  Please ask
# me questions if you have any, I'm happy to share and "Make" is one
# of my favourite languages.

##
# Set up the build environment and settings
##
SHELL = /bin/bash
#COLOR = $(shell echo $$TERM|grep color > /dev/null && echo always || echo auto)
COLOR = always
CXX = clang++
LanguageVersion = -std=c++20
Warnings = -Wall -Werror -Wextra -Wshadow
NoWarn = -Wno-dangling-else -Wno-c++98-compat -Wno-padded -Wno-missing-prototypes -Wno-dangling-else -Wno-old-style-cast -Wno-unused-macros -Wno-comma -Wno-return-std-move
#SANS=address
ifeq ($(SANS),)
	SANITIZER=
else
	SANITIZER=-fsanitize=$(SANS) -ftrapv
endif

Debugging = -Wfatal-errors -fdiagnostics-color=$(COLOR) -g $(SANITIZER)
CXXFLAGS = $(LanguageVersion) $(Warnings) $(NoWarn) $(Debugging)

HEADER_FILES  = $(wildcard *.h) $(wildcard *.hpp)
CCODE_FILES   = $(wildcard *.c)
CPPCODE_FILES = $(wildcard *.cpp)
CODE_FILES    = $(CCODE_FILES) $(CPPCODE_FILES)

##
# Describe the actual program structure.
# These This is the only important line for building the program.
##
simplifyer: simplifyer.o simplify.o parse.o equation.o
	$(CXX) $(CXXFLAGS) -o $@ $^
parser: parser.o parse.o equation.o
	$(CXX) $(CXXFLAGS) -o $@ $^

all: parser
clean_targets:
	-rm parser

##
# Code to check for `#include' statements.
##
DEPDIR := .dep
$(DEPDIR): ; mkdir $@
.PRECIOUS: $(DEPDIR)/%.d
$(DEPDIR)/%.d: %.cpp | $(DEPDIR)
	@set -e; rm -f $@; \
	$(CXX) -MM $(CXXFLAGS) $< -MF $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$
$(DEPDIR)/%.d: %.c | $(DEPDIR)
	@set -e; rm -f $@; \
	$(CC) -MM $(CXXFLAGS) $< -MF $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$
%.o: .dep/%.d
DEPFILES := $(CCODE_FILES:%.c=$(DEPDIR)/%.d) $(CPPCODE_FILES:%.cpp=$(DEPDIR)/%.d)
include $(DEPFILES)

##
# Tests are run from the tests.d directory and temporary results are stored in the tests.r directory
##
%.d: ; mkdir $@
TEST_DIR = tests.d
TEST_RES = tests.r
TEST_FILES   = $(wildcard $(TEST_DIR)/*)
$(TEST_RES): ; mkdir $@

##
# Make a pseudo-target for the tests
##
# Also, make a happy message at the end if they all passed.
.PHONY: tests
tests:
	@echo "+------------------------------------------------+"
	@echo "| All tests passed and All documenation is done. |"
	@echo "+------------------------------------------------+"

##
# Very small "interactive debugger"
##
.PHONY: watch
watch:
	watch --color "$(MAKE) _watch_wait && $(MAKE) tests"
.PHONY: _watch_wait
# This is a separate directive so that the lists update.
IMPORTANT_FILES = $(HEADER_FILES) $(CODE_FILES) $(TEST_FILES)
_watch_wait:
	@inotifywait $(IMPORTANT_FILES)

##
# Find TODO tags and display them at the top of the output for convenience
##
.PHONY: TODO
tests: TODO
# TODO: Don't display this if there are no TODOs
# TODO: Better sort on line numbers.
TODO:
	@echo "+---------------------------- - - -  -   --    ---"
	@echo "| List of TODO in code:"
	@echo "|"
	@grep --color=$(COLOR) -n "\(TODO\|FIXME\)" $(HEADER_FILES) $(CODE_FILES) |sort |sed 's,\(.\),| - \1,' | sed 's,\s*//\s*,\t,' || true
	@grep --color=$(COLOR) -n MAINTENANCE $(HEADER_FILES) $(CODE_FILES) |sort |sed 's,\(.\),| - \1,' | sed 's,\s*//\s*,\t,' || true
	@echo "+---------------------------- - - -  -   --    ---"
	@echo ""

##
# Run the tests
##
# Tests are of the form $(TEST_DIR)/number-progam-testname.input $(TEST_DIR)/number-progam-testname.output
##
# Run the tests - by making a list of tests to run.
##
# Tests are of the form $(TEST_DIR)/number-progam-testname.input $(TEST_DIR)/number-progam-testname.output
testrunner: $(TEST_FILES) Makefile tests.org | $(TEST_DIR)
	@echo "Building test files"
	# TODO: Move the test dir, update it, move it back
	# This will keep the old timestamps if there was no diff and prevent rerunning tests
	-@rm $(TEST_DIR)/*
	@emacs --batch tests.org --eval "(setq org-src-preserve-indentation t)" -f org-babel-tangle
	@echo "Building tests list"
	@ls tests.d|sed 's,\([0-9]*-[a-zA-Z_]*\)-[a-zA-Z0-9_]*[.]\(in\|out\)put,\1,'|uniq|sort|egrep "^[0-9]*-[a-zA-Z_]*$$" > $@
	@sed -i $@ -e 's,\([0-9]*\)-\(.*\),tests: $(TEST_RES)/\1-\2\n$(TEST_RES)/\1-\2: \2 $(TEST_DIR)/\1-\2-*.input $(TEST_DIR)/\1-\2-*.output | $(TEST_RES)\n\t./\2 $(TEST_DIR)/\1-\2-*.input > $(TEST_RES)/\1-\2.tmp\n\tdiff -u $(TEST_RES)/\1-\2.tmp $(TEST_DIR)/\1-\2-*.output | ydiff --color=always --pager=cat\n\t@diff -u $(TEST_RES)/\1-\2.tmp $(TEST_DIR)/\1-\2-*.output > /dev/null\n\tmv $(TEST_RES)/\1-\2.tmp tests.r/\1-\2,'
# TODO: This warns on all of the recipes being refreshed, silence that somehow.
include testrunner

##
# Give a list of missing documentation
##
Doxyfile:
	doxygen -g > /dev/null
tests: $(TEST_RES)/docs
$(TEST_RES)/docs: $(wildcard *.h) $(wildcard *.cpp) Doxyfile Makefile
	@echo "+------------------------------------------------+"
	@echo "| Tests Compleate                                |"
	@echo '| Checking for missing Documentation             |'
	@echo "+------------------------------------------------+"
	@doxygen 2>&1 | grep "warning" | sed 's,.*/,,' | tee $@.tmp | head -n 30
	@diff $@.tmp /dev/null > /dev/null
	@mv $@.tmp $@

##
# Remove the files made by this Makefile
##
clean: clean_targets
	-rm *.o
	-rm Doxyfile doxygen* docs docs.tmp
	-rm -rf html latex
	-rm -rf .dep
	-rm -rf $(TEST_RES)
