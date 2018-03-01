MYSRC := $(shell pwd)/mysql-src
MYBUILD := $(MYSRC)/build
RPATH := 1

CXX := g++-4.7
MYSQL_PLUGIN_DIR := /usr/lib/mysql/plugin


OBJDIR	 := obj
TOP	 := $(shell echo $${PWD-`pwd`})
#CXX	 := g++
AR	 := ar
## -g -O0 -> -O2
CXXFLAGS := -g -O0 -fno-strict-aliasing -fno-rtti -fwrapv -fPIC \
	    -Wall -Werror -Wpointer-arith -Wendif-labels -Wformat=2  \
	    -Wextra -Wmissing-noreturn -Wwrite-strings -Wno-unused-parameter \
	    -Wno-deprecated \
	    -Wmissing-declarations -Woverloaded-virtual  \
	    -Wunreachable-code -D_GNU_SOURCE -std=c++0x -I$(TOP)
LDFLAGS  := -L$(TOP)/$(OBJDIR) -Wl,--no-undefined


## Use RPATH only for debug builds; set RPATH=1 in config.mk.
ifeq ($(RPATH),1)
LDRPATH	 := -Wl,-rpath=$(TOP)/$(OBJDIR) -Wl,-rpath=$(TOP)
endif

CXXFLAGS += -I$(MYBUILD)/include \
	    -I$(MYSRC)/include \
	    -I$(MYSRC)/sql \
	    -I$(MYSRC)/regex \
	    -I$(MYBUILD)/sql \
	    -DHAVE_CONFIG_H -DMYSQL_SERVER -DEMBEDDED_LIBRARY -DDBUG_OFF \
	    -DMYSQL_BUILD_DIR=\"$(MYBUILD)\"
LDFLAGS	 += -lpthread -lrt -ldl -lcrypt -lreadline

## To be populated by Makefrag files

OBJDIRS	:=

.PHONY: all
all:

.PHONY: install
install:

.PHONY: clean
clean:
	rm -rf $(OBJDIR) mtl

.PHONY: doc
doc:
	doxygen CryptDBdoxgen

.PHONY: whitespace
whitespace:
	find . -name '*.cc' -o -name '*.hh' -type f -exec sed -i 's/ *$//' '{}' ';'

.PHONY: always
always:

.PHONY: MYTOKEN

# Eliminate default suffix rules
.SUFFIXES:

# Delete target files if there is an error (or make is interrupted)
.DELETE_ON_ERROR:

# make it so that no intermediate .o files are ever deleted
.PRECIOUS: %.o

$(OBJDIR)/%.o: %.cc
	@mkdir -p $(@D)
	$(CXX) -MD $(CXXFLAGS) -c $< -o $@

$(OBJDIR)/%.o: $(OBJDIR)/%.cc
	@mkdir -p $(@D)
	$(CXX) -MD $(CXXFLAGS) -c $< -o $@

##rules for .cc in debug directory

mtl/%:$(OBJDIR)/debug/%.o token.o
	@mkdir -p $(@D)
	$(CXX) -g -o $@ $^ $(CXXFLAGS) $(LDFLAGS)  -L/$(MYBUILD)/libmysqld -lmysqld -laio -lz -ldl -lm -lcrypt -lpthread -lwrapper  -lcryptdb -ledbcrypto -ledbutil -ledbparser -lntl -lcrypto

token.o:$(OBJDIR)/libwrapper.so
	-rm token.o
	$(CXX) -g -c token.cc


mtl/test_wrapper_exe/%:$(OBJDIR)/test_wrapper/%.o token.o
	@mkdir -p $(@D)
	$(CXX) -g -o $@ $^ $(CXXFLAGS) $(LDFLAGS)  -L/$(MYBUILD)/libmysqld -lmysqld -laio -lz -ldl -lm -lcrypt -lpthread -lwrapper  -lcryptdb -ledbcrypto -ledbutil -ledbparser -lntl -lcrypto

mtl/test_util_exe/%:$(OBJDIR)/test_util/%.o
	@mkdir -p $(@D)
	$(CXX) -g -o $@ $^ $(CXXFLAGS) $(LDFLAGS)  -L/$(MYBUILD)/libmysqld -lmysqld -laio -lz -ldl -lm -lcrypt -lpthread -lcryptdb -ledbcrypto -ledbutil -ledbparser -lntl -lcrypto

mtl/test_parser_exe/%:$(OBJDIR)/test_parser/%.o
	@mkdir -p $(@D)
	$(CXX) -g -o $@ $^ $(CXXFLAGS) $(LDFLAGS)  -L/$(MYBUILD)/libmysqld -lmysqld -laio -lz -ldl -lm -lcrypt -lpthread -lcryptdb -ledbcrypto -ledbutil -ledbparser -lntl -lcrypto

mtl/test_main_exe/%:$(OBJDIR)/test_main/%.o
	@mkdir -p $(@D)
	$(CXX) -g -o $@ $^ $(CXXFLAGS) $(LDFLAGS)  -L/$(MYBUILD)/libmysqld -lmysqld -laio -lz -ldl -lm -lcrypt -lpthread -lcryptdb -ledbcrypto -ledbutil -ledbparser -lntl -lcrypto




include crypto/Makefrag
include parser/Makefrag
include main/Makefrag
include util/Makefrag
include udf/Makefrag
include mysqlproxy/Makefrag
include debug/Makefrag
include test_wrapper/Makefrag
include test_util/Makefrag
include test_parser/Makefrag
include test_main/Makefrag
include wrapper/Makefrag
include test_parser_helper/Makefrag

$(OBJDIR)/.deps: $(foreach dir, $(OBJDIRS), $(wildcard $(OBJDIR)/$(dir)/*.d))
	@mkdir -p $(@D)
	perl mergedep.pl $@ $^
	echo "after merge"
-include $(OBJDIR)/.deps

