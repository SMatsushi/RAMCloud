# This makefile system follows the structuring conventions
# recommended by Peter Miller in his excellent paper:
#
#       Recursive Make Considered Harmful
#       http://aegis.sourceforge.net/auug97.pdf

# --- private/MakefragPrivateTop for dpdk
GCC44 := /usr/bin/gcc44
GXX44 := /usr/bin/g++44
GCC   := /usr/bin/gcc
GXX   := /usr/bin/g++

DEBUG    ?= yes
COMPILER ?= gnu

## configurable parameters:
# DpdkDriver
DPDK        ?= no

ifeq ($(DPDK),yes)

ifeq ($(DEBUG),yes)
TIME_ATTACK := no
else
# ClusterPerf time attack mode. display fastest lap instead of average time
TIME_ATTACK ?= yes
endif

WORKAROUND_THREAD := yes
WORKAROUND_CYCLES := yes

else
TIME_ATTACK       := no
WORKAROUND_THREAD := no
WORKAROUND_CYCLES := no
endif

ifeq ($(COMPILER),gnu)

# use gcc44, g++44 if available.
# On CentOS7, install compat-gcc-44 and compat-gcc-44-c++ packages.
CC  := $(shell test -x $(GCC44) && echo $(GCC44) || echo $(GCC))
CXX := $(shell test -x $(GXX44) && echo $(GXX44) || echo $(GXX))

$(info CC  = $(CC) $(shell $(CC) -dumpversion))
$(info CXX = $(CXX) $(shell $(CXX) -dumpversion))

GCC_MAJOR_VERSION = $(shell $(CC) -dumpversion | cut -f1 -d.)
GCC_MINOR_VERSION = $(shell $(CC) -dumpversion | cut -f2 -d.)

ifeq ($(shell test $(GCC_MAJOR_VERSION) -lt 4 && echo 1),1)
$(error GCC 4 or later required)
endif

ifeq ($(shell test $(GCC_MINOR_VERSION) -ge 6 && echo 1), 1)
# corei7 support added since gcc-4.6.x
ARCH ?= corei7
else
ARCH ?= atom
endif
TUNE ?= $(ARCH)
endif
# --- private/MakefragPrivateTop for dpdk - end

# The following line allows developers to change the default values for make
# variables in the file private/MakefragPrivateTop.
include $(wildcard private/MakefragPrivateTop)

DEBUG ?= yes
YIELD ?= no
SSE ?= sse4.2
COMPILER ?= gnu
VALGRIND ?= no
ONLOAD_DIR ?= /usr/local/openonload-201405

## Create a separate build directory for each git branch and for each arch
OBJSUFFIX := $(shell git symbolic-ref -q HEAD | \
	       sed -e s,refs/heads/,.,)

OBJDIR	:= obj$(OBJSUFFIX)

TOP	:= $(shell echo $${PWD-`pwd`})
GTEST_DIR ?= $(TOP)/gtest

# Determines whether or not RAMCloud is built with support for LogCabin as an
# ExternalStorage implementation.
LOGCABIN ?= no
ifeq ($(LOGCABIN),yes)
LOGCABIN_LIB ?= logcabin/build/liblogcabin.a
LOGCABIN_DIR ?= logcabin
else
LOGCABIN_LIB :=
LOGCABIN_DIR :=
endif

# Determines whether or not RAMCloud is built with support for ZooKeeper as an
# ExternalStorage implementation.
ZOOKEEPER ?= yes
ifeq ($(ZOOKEEPER),yes)
ZOOKEEPER_LIB ?= /usr/local/lib/libzookeeper_mt.a
ZOOKEEPER_DIR ?= /usr/local/zookeeper-3.4.5
else
ZOOKEEPER_LIB :=
ZOOKEEPER_DIR :=
endif


ifeq ($(DEBUG),yes)
BASECFLAGS := -g
OPTFLAG	 :=
DEBUGFLAGS := -DTESTING=1 -fno-builtin
else
BASECFLAGS := -g
OPTFLAG := -O3
DEBUGFLAGS := -DNDEBUG -Wno-unused-variable
endif

COMFLAGS := $(BASECFLAGS) $(OPTFLAG) -fno-strict-aliasing \
	        -MD -m$(SSE) \
	        $(DEBUGFLAGS)
ifeq ($(COMPILER),gnu)
COMFLAGS += -march=core2
endif
ifeq ($(VALGRIND),yes)
COMFLAGS += -DVALGRIND
endif
ifeq ($(LOGCABIN),yes)
COMFLAGS += -DENABLE_LOGCABIN
endif
ifeq ($(ZOOKEEPER),yes)
COMFLAGS += -DENABLE_ZOOKEEPER
endif

COMWARNS := -Wall -Wformat=2 -Wextra \
            -Wwrite-strings -Wno-unused-parameter -Wmissing-format-attribute
CWARNS   := $(COMWARNS) -Wmissing-prototypes -Wmissing-declarations -Wshadow \
		-Wbad-function-cast
CXXWARNS := $(COMWARNS) -Wno-non-template-friend -Woverloaded-virtual \
		-Wcast-qual \
		-Wcast-align -Wconversion
ifeq ($(COMPILER),gnu)
CXXWARNS += -Weffc++
endif
# Too many false positives list:
# -Wunreachable-code
# Failed deconstructor inlines are generating noise
# -Winline

LIBS := $(EXTRALIBS) $(LOGCABIN_LIB) $(ZOOKEEPER_LIB) \
	-lpcrecpp -lboost_program_options \
	-lprotobuf -lrt -lboost_filesystem -lboost_system \
	-lpthread -lssl -lcrypto
ifeq ($(DEBUG),yes)
# -rdynamic generates more useful backtraces when you have debugging symbols
LIBS += -rdynamic
endif

INCLUDES := -I$(TOP)/src \
            -I$(TOP)/$(OBJDIR) \
            -I$(GTEST_DIR)/include \
            -I/usr/local/openonload-201405/src/include \
             $(NULL)
ifeq ($(LOGCABIN),yes)
INCLUDES := $(INCLUDES) -I$(LOGCABIN_DIR)/include
endif

CC ?= gcc
CXX ?= g++
AR ?= ar
PERL ?= perl
PYTHON ?= python
LINT := $(PYTHON) cpplint.py --filter=-runtime/threadsafe_fn,-readability/streams,-whitespace/blank_line,-whitespace/braces,-whitespace/comments,-runtime/arrays,-build/include_what_you_use,-whitespace/semicolon
PRAGMAS := ./pragmas.py
NULL := # useful for terminating lists of files
PROTOC ?= protoc
EPYDOC ?= epydoc
EPYDOCFLAGS ?= --simple-term -v
DOXYGEN ?= doxygen

# Directory for installation: various subdirectories such as include and
# bin will be created by "make install".
INSTALL_DIR ?= install

# Check if OnLoad is installed on the system. OnLoad is required to build
# SolarFlare driver code.
ONLOAD_VERSION := $(shell $(ONLOAD_DIR)/scripts/onload --version 2>/dev/null)
ifdef ONLOAD_VERSION
	ONLOAD = yes
	ONLOAD_LIB := -L$(ONLOAD_DIR)/build/gnu_x86_64/lib/ciul/ -L$(ONLOAD_DIR)/build/gnu_x86_64/lib/citools/ -lcitools1 -lciul1
	LIBS += $(ONLOAD_LIB)
	COMFLAGS += -DONLOAD
endif

# Test whether Infiniband support is available. Avoids using $(COMFLAGS)
# (particularly, -MD) which results in bad interactions with mergedeps.
INFINIBAND = $(shell $(CXX) $(INCLUDES) $(EXTRACXXFLAGS) $(LIBS) -libverbs \
                         -o /dev/null src/HaveInfiniband.cc \
                         >/dev/null 2>&1 \
                         && echo yes || echo no)

ifeq ($(INFINIBAND),yes)
COMFLAGS += -DINFINIBAND
LIBS += -libverbs
endif

# DPDK
ifeq ($(DPDK),yes)
RTE_TARGET  ?= x86_64-native-linuxapp-gcc
COMFLAGS    += -DDPDK

ifeq ($(RTE_SDK),)
# link with the libraries installed on the system
ifeq ($(DPDK_SHARED),no)
$(error DPDK_SHARED should be yes when linking libraries installed on the system)
endif
DPDK_SHARED := yes

else
# link with the libraries in the dpdk sdk under RTE_SDK
ifeq ($(wildcard $(RTE_SDK)),)
$(error RTE_SDK variable points to an invalid location)
endif
ifeq ($(wildcard $(RTE_SDK)/$(RTE_TARGET)),)
$(error $(RTE_SDK)/$(RTE_TARGET) not found. build and install the DPDK SDK first.)
endif

DPDK_SHARED ?= no
RTE_INCDIR := $(RTE_SDK)/$(RTE_TARGET)/include
RTE_LIBDIR := $(RTE_SDK)/$(RTE_TARGET)/lib
COMFLAGS   += -I$(RTE_INCDIR)
LIBS       += -L$(RTE_LIBDIR)

# end of RTE_SDK
endif

ifeq ($(DPDK_SHARED),yes)
# link with the shared libraries
## dpdk shared libraries.
RTE_SHLIBS := -lethdev -lrte_mbuf -lrte_malloc -lrte_mempool
RTE_SHLIBS += -lrte_ring -lrte_kvargs -lrte_eal
## poll mode drivers, depends on dpdk configuration.
RTE_SHLIBS += -lrte_pmd_e1000 -lrte_pmd_ixgbe
RTE_SHLIBS += -lrte_pmd_virtio_uio -lrte_pmd_ring
## -ldl required because librte_eal refers to dlopen()
LIBS += $(RTE_SHLIBS) -ldl
else
# link with the static link library
## assume dpdk sdk is build with CONFIG_RTE_BUILD_COMBINE_LIBS=y and -fPIC
RTE_ARLIBS := $(RTE_LIBDIR)/libintel_dpdk.a
## --whole-archive is required to link the pmd objects.
LIBS += -Wl,--whole-archive $(RTE_ARLIBS) -Wl,--no-whole-archive -ldl
endif

# end of DPDK
endif

ifeq ($(YIELD),yes)
COMFLAGS += -DYIELD=1
endif

CFLAGS_BASE := $(COMFLAGS) -std=gnu0x $(INCLUDES)
CFLAGS_SILENT := $(CFLAGS_BASE)
CFLAGS_NOWERROR := $(CFLAGS_BASE) $(CWARNS)
CFLAGS := $(CFLAGS_BASE) -Werror $(CWARNS)

CXXFLAGS_BASE := $(COMFLAGS) -std=c++0x $(INCLUDES)
CXXFLAGS_SILENT := $(CXXFLAGS_BASE) $(EXTRACXXFLAGS)
CXXFLAGS_NOWERROR := $(CXXFLAGS_BASE) $(CXXWARNS) $(EXTRACXXFLAGS)
CXXFLAGS := $(CXXFLAGS_BASE) -Werror $(CXXWARNS) $(EXTRACXXFLAGS) $(PERF)

ifeq ($(COMPILER),intel)
CXXFLAGS = $(CXXFLAGS_BASE) $(CXXWARNS)
endif

# run-cc:
# Compile a C source file to an object file.
# Uses the GCCWARN pragma setting defined within the C source file.
# The first parameter $(1) should be the output filename (*.o)
# The second parameter $(2) should be the input filename (*.c)
# The optional third parameter $(3) is any additional options compiler options.
define run-cc
@GCCWARN=$$( $(PRAGMAS) -q GCCWARN $(2) ); \
case $$GCCWARN in \
0) \
	echo $(CC) $(CFLAGS_SILENT) $(3) -c -o $(1) $(2); \
	$(CC) $(CFLAGS_SILENT) $(3) -c -o $(1) $(2); \
	;; \
5) \
	echo $(CC) $(CFLAGS_NOWERROR) $(3) -c -o $(1) $(2); \
	$(CC) $(CFLAGS_NOWERROR) $(3) -c -o $(1) $(2); \
	;; \
9) \
	echo $(CC) $(CFLAGS) $(3) -c -o $(1) $(2); \
	$(CC) $(CFLAGS) $(3) -c -o $(1) $(2); \
	;; \
esac
endef

# run-cxx:
# Compile a C++ source file to an object file.
# Uses the GCCWARN pragma setting defined within the C source file.
# The first parameter $(1) should be the output filename (*.o)
# The second parameter $(2) should be the input filename (*.cc)
# The optional third parameter $(3) is any additional options compiler options.
define run-cxx
@GCCWARN=$$( $(PRAGMAS) -q GCCWARN $(2) ); \
case $$GCCWARN in \
0) \
	echo $(CXX) $(CXXFLAGS_SILENT) $(3) -c -o $(1) $(2); \
	$(CXX) $(CXXFLAGS_SILENT) $(3) -c -o $(1) $(2); \
	;; \
5) \
	echo $(CXX) $(CXXFLAGS_NOWERROR) $(3) -c -o $(1) $(2); \
	$(CXX) $(CXXFLAGS_NOWERROR) $(3) -c -o $(1) $(2); \
	;; \
9) \
	echo $(CXX) $(CXXFLAGS) $(3) -c -o $(1) $(2); \
	$(CXX) $(CXXFLAGS) $(3) -c -o $(1) $(2); \
	;; \
esac
endef

all:

tests: test
test: python-test

.SUFFIXES:

include src/Makefrag
include src/MakefragClient
include src/MakefragServer
include src/MakefragCoordinator
include src/MakefragTest
include src/misc/Makefrag
include bindings/python/Makefrag

# --- private/MakefragPrivate for dpdk
ifeq ($(COMPILER),gnu)
ifeq ($(shell test $(GCC_MINOR_VERSION) -ge 8 && echo 1), 1)
# tentatively accept the warnings to build with gcc-4.8 or later
CFLAGS   := $(filter-out -Werror, $(CFLAGS))
CXXFLAGS := $(filter-out -Werror, $(CXXFLAGS))
endif
endif

ifneq ($(ARCH),)
CFLAGS   := $(filter-out -march=core2, $(CFLAGS))   -march=$(ARCH)
CXXFLAGS := $(filter-out -march=core2, $(CXXFLAGS)) -march=$(ARCH)
endif
ifneq ($(TUNE),)
CFLAGS   += -mtune=$(TUNE)
CXXFLAGS += -mtune=$(TUNE)
endif

ifeq ($(TIME_ATTACK),yes)
CFLAGS   += -DTIME_ATTACK
CXXFLAGS += -DTIME_ATTACK
endif

ifeq ($(WORKAROUND_THREAD),yes)
CFLAGS   += -DWORKAROUND_THREAD
CXXFLAGS += -DWORKAROUND_THREAD
endif
ifeq ($(WORKAROUND_CYCLES),yes)
CFLAGS   += -DWORKAROUND_CYCLES
CXXFLAGS += -DWORKAROUND_CYCLES
endif
# --- private/MakefragPrivate for dpdk - end

# The following line allows developers to create private make rules
# in the file private/MakefragPrivate.  The recommended approach is
# for you to keep all private files (personal development hacks,
# test scripts, etc.) in the "private" subdirectory.
include $(wildcard private/MakefragPrivate)

clean: tests-clean docs-clean tags-clean install-clean java-clean
	rm -rf $(OBJDIR)/.deps $(OBJDIR)/*

check:
	$(LINT) $$(./pragmas.py -f CPPLINT:5 $$(find $(TOP)/src '(' -name '*.cc' -or -name '*.h' -or -name '*.c' ')' -not -path '$(TOP)/src/btree/*' -not -path '$(TOP)/src/btreeRamCloud/*'))

# This magic automatically generates makefile dependencies
# for header files included from C source files we compile,
# and keeps those dependencies up-to-date every time we recompile.
# See 'mergedep.pl' for more information.
OBJDIRS += .
$(OBJDIR)/.deps: $(foreach dir, $(OBJDIRS), $(wildcard $(OBJDIR)/$(dir)/*.d))
	@mkdir -p $(@D)
	$(PERL) mergedep.pl $@ $^

-include $(OBJDIR)/.deps

always:
	@:

doc: docs
# Get the branch name and SHA from git and put that into the doxygen mainpage
docs:
	@DOCSID=`git branch --no-color | grep "*" | cut -f2 -d" "` ;\
	DOCSID=$$DOCSID-`cat ".git/$$( git symbolic-ref HEAD )" | cut -c1-6` ;\
	(echo "PROJECT_NUMBER = \"Version [$$DOCSID]\""; \
	 echo "INPUT = src bindings README $(OBJDIR)"; \
	 echo "INCLUDE_PATH = $(OBJDIR)"; ) | cat Doxyfile - | $(DOXYGEN) -

docs-clean: python-docs-clean
	rm -rf docs/doxygen/

tags: 
	find . -type f | grep -v "\.git" | grep -v docs | xargs etags
	find . -type f | grep -v "\.git" | grep -v docs | xargs ctags

tags-clean:
	rm -f TAGS tags

# The following target is useful for debugging Makefiles; it
# prints the value of a make variable.
print-%:
	@echo $* = $($*)

# Rebuild the Java bindings
java: $(OBJDIR)/libramcloud.a
	cd bindings/java; ./gradlew build
java-clean:
	-cd bindings/java; ./gradlew clean

INSTALL_BINS := \
    $(OBJDIR)/client \
    $(OBJDIR)/coordinator \
    $(OBJDIR)/ensureServers \
    $(OBJDIR)/libramcloud.so \
    $(OBJDIR)/server \
    $(NULL)

INSTALL_LIBS := \
    $(OBJDIR)/libramcloud.a \
    $(NULL)

# The header files below are those that must be installed in order to
# compile RAMCloud applications. Please try to keep this list as short
# as possible.
INSTALL_INCLUDES := \
    src/Atomic.h \
    src/BoostIntrusive.h \
    src/Buffer.h \
    src/ClientException.h \
    src/CodeLocation.h \
    src/CoordinatorClient.h \
    src/CoordinatorRpcWrapper.h \
    src/Crc32C.h \
    src/Exception.h \
    src/Fence.h \
    src/Key.h \
    src/IndexRpcWrapper.h \
    src/LinearizableObjectRpcWrapper.h \
    src/LogEntryTypes.h \
    src/Logger.h \
    src/LogMetadata.h \
    src/MasterClient.h \
    src/Minimal.h \
    src/Object.h \
    src/ObjectBuffer.h \
    src/ObjectRpcWrapper.h \
    src/PerfStats.h \
    src/RamCloud.h \
    src/RpcWrapper.h \
    src/RpcTracker.h \
    src/RejectRules.h \
    src/ServerId.h \
    src/ServerIdRpcWrapper.h \
    src/ServerMetrics.h \
    src/ServiceMask.h \
    src/SpinLock.h \
    src/Status.h \
    src/TestLog.h \
    src/Transport.h \
    src/Tub.h \
    src/WireFormat.h \
    $(OBJDIR)/Histogram.pb.h \
    $(OBJDIR)/Indexlet.pb.h \
    $(OBJDIR)/LogMetrics.pb.h \
    $(OBJDIR)/MasterRecoveryInfo.pb.h \
    $(OBJDIR)/RecoveryPartition.pb.h \
    $(OBJDIR)/ServerConfig.pb.h \
    $(OBJDIR)/ServerList.pb.h \
    $(OBJDIR)/ServerStatistics.pb.h \
    $(OBJDIR)/SpinLockStatistics.pb.h \
    $(OBJDIR)/TableConfig.pb.h \
    $(OBJDIR)/Tablets.pb.h \
    $(NULL)

INSTALLED_BINS := $(patsubst $(OBJDIR)/%, $(INSTALL_DIR)/bin/%, $(INSTALL_BINS))
INSTALLED_LIBS := $(patsubst $(OBJDIR)/%, $(INSTALL_DIR)/lib/%, $(INSTALL_LIBS))
	
install: all
	mkdir -p $(INSTALL_DIR)/bin
	cp $(INSTALL_BINS) $(INSTALL_DIR)/bin
	mkdir -p $(INSTALL_DIR)/include/ramcloud
	cp $(INSTALL_INCLUDES) $(INSTALL_DIR)/include/ramcloud
	mkdir -p $(INSTALL_DIR)/lib/ramcloud
	cp $(INSTALL_LIBS) $(INSTALL_DIR)/lib/ramcloud

java-install: java
	mkdir -p $(INSTALL_DIR)/bin
	rm -rf $(INSTALL_DIR)/bin/java
	cp -r bindings/java/bin $(INSTALL_DIR)/bin/java
	rm -rf $(INSTALL_DIR)/lib/ramcloud/java
	cp -r bindings/java/lib $(INSTALL_DIR)/lib/ramcloud/java

install-clean:
	rm -rf install

logcabin:
	cd logcabin; \
	scons

startZoo:
	if [ ! -e $(ZOOKEEPER_DIR)/data/zookeeper_server.pid ]; then \
	        $(ZOOKEEPER_DIR)/bin/zkServer.sh start; fi

stopZoo:
	$(ZOOKEEPER_DIR)/bin/zkServer.sh stop

.PHONY: all always clean check doc docs docs-clean install tags tags-clean \
	test tests logcabin startZoo stopZoo
