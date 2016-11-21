# Copyright (C) Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.

#------------------------------------------------------------------------------
# Makefile
#------------------------------------------------------------------------------
# Building the swoop project
#
COMPILER_DIR=$(shell pwd)
BUILD_DIR=$(COMPILER_DIR)/build

LLVM_DIR=$(COMPILER_DIR)/llvm
LLVM_BUILD_DIR=$(BUILD_DIR)/llvm-build
LLVM_LIB_DIR=$(LLVM_BUILD_DIR)/lib
LLVM_BUILD_MAKEFILE=$(LLVM_BUILD_DIR)/Makefile
LLVM_CMAKE_PATH=$(LLVM_BUILD_DIR)/share/llvm/cmake/

CLANG_LIB=$(LLVM_LIB_DIR)/libclang.so

BUILD_TYPE?=MinSizeRel

GIT_MAJOR:=$(shell git --version | cut -d ' ' -f 3 | cut -d '.' -f 1)
ifeq ($(shell test $(GIT_MAJOR) -ge 2; echo $$?),0)
  GIT_SHALLOW=1
else
  GIT_SHALLOW=0
endif

#------------------------------------------------------------------------------
# LLVM tools: Clang, Compiler-rt
#------------------------------------------------------------------------------
# 

LLVM_REPOSITORY=https://github.com/llvm-mirror/llvm.git
LLVM_RELEASE=release_38

LLVM_TOOLS=$(LLVM_DIR)/tools
CLANG=$(LLVM_TOOLS)/clang
CLANG_REPOSITORY=http://llvm.org/git/clang.git

LLVM_PROJECTS=$(LLVM_DIR)/projects
COMPILER_RT=$(LLVM_PROJECTS)/compiler-rt
COMPILER_RT_REPOSITORY=http://llvm.org/git/compiler-rt.git

OMP=$(LLVM_PROJECTS)/openmp
OMP_REPOSITORY=https://github.com/llvm-mirror/openmp.git

TOOL_RELEASE=release_38
TARGETS_TO_BUILD=X86
#TARGETS_TO_BUILD="X86;ARM"

#------------------------------------------------------------------------------
# Projects
#------------------------------------------------------------------------------
# 
PROJECTS_DIR=$(COMPILER_DIR)
PROJECTS_BUILD_DIR=$(BUILD_DIR)/projects-build
PROJECTS_BUILD_MAKEFILE=$(PROJECTS_BUILD_DIR)/Makefile

#------------------------------------------------------------------------------
# OTHERS
#------------------------------------------------------------------------------
#

all: $(PROJECTS_BUILD_MAKEFILE) 
	$(MAKE) -C $(PROJECTS_BUILD_DIR)

$(CLANG_LIB): $(LLVM_BUILD_MAKEFILE)
	$(MAKE) -C $(LLVM_BUILD_DIR)

$(LLVM_BUILD_MAKEFILE): $(LLVM_DIR)/Makefile $(CLANG) $(COMPILER_RT) $(OMP)
	mkdir -p $(LLVM_BUILD_DIR)
	cd $(LLVM_BUILD_DIR); cmake -D CMAKE_BUILD_TYPE=$(BUILD_TYPE) -DLLVM_TARGETS_TO_BUILD=$(TARGETS_TO_BUILD) -DLLVM_BINUTILS_INCDIR=/usr/include $(LLVM_DIR);

$(LLVM_DIR)/Makefile:
ifeq ($(GIT_SHALLOW),1)
#	# Disable shallow cloning of a specific version
#	# This should be fixed with Git 2.5+
#	# From http://stackoverflow.com/a/30701724 http://stackoverflow.com/questions/14872486/pull-a-specific-commit-from-a-remote-git-repository
	git clone -b $(LLVM_RELEASE) --depth 1 $(LLVM_REPOSITORY) $(LLVM_DIR);
#	# From http://stackoverflow.com/questions/3489173/how-to-clone-git-repository-with-specific-revision-changeset
# 	mkdir $(LLVM_DIR)
# 	git init $(LLVM_DIR)
# 	git --git-dir=$(LLVM_DIR)/.git --work-tree=$(LLVM_DIR) remote add origin $(LLVM_REPOSITORY)
#	git --git-dir=$(LLVM_DIR)/.git --work-tree=$(LLVM_DIR) fetch --depth 1 origin $(LLVM_RELEASE)
#	git --git-dir=$(LLVM_DIR)/.git --work_tree=$(LLVM_DIR) reset --hard FETCH_HEAD
else
	git clone $(LLVM_REPOSITORY) $(LLVM_DIR);
	cd $(LLVM_DIR); git checkout $(LLVM_RELEASE);
endif

$(CLANG): $(LLVM_DIR)/Makefile
ifeq ($(GIT_SHALLOW),1)
	cd $(LLVM_TOOLS); git clone -b $(TOOL_RELEASE) --depth 1 $(CLANG_REPOSITORY);
else
	cd $(LLVM_TOOLS); git clone $(CLANG_REPOSITORY);
	cd $(CLANG); git checkout $(TOOL_RELEASE);
endif

$(COMPILER_RT): $(LLVM_DIR)/Makefile
ifeq ($(GIT_SHALLOW),1)
	cd $(LLVM_PROJECTS); git clone -b $(TOOL_RELEASE) --depth 1 $(COMPILER_RT_REPOSITORY);
else
	cd $(LLVM_PROJECTS); git clone $(COMPILER_RT_REPOSITORY);
	cd $(COMPILER_RT); git checkout $(TOOL_RELEASE);
endif

$(OMP): $(LLVM_DIR)/Makefile
ifeq ($(GIT_SHALLOW),1)
	cd $(LLVM_PROJECTS); git clone -b $(TOOL_RELEASE) --depth 1 $(OMP_REPOSITORY);
else
	cd $(LLVM_PROJECTS); git clone $(OMP_REPOSITORY);
	cd $(OMP); git checkout $(TOOL_RELEASE);
endif


$(PROJECTS_BUILD_MAKEFILE): $(CLANG_LIB)
	mkdir -p $(PROJECTS_BUILD_DIR)
	cd $(PROJECTS_BUILD_DIR); cmake -DLLVM_DIR:STRING=$(LLVM_CMAKE_PATH) -D CMAKE_BUILD_TYPE=$(BUILD_TYPE) -DLLVM_TARGETS_TO_BUILD=$(TARGETS_TO_BUILD) $(PROJECTS_DIR);
clean:
	rm -rf $(PROJECTS_BUILD_DIR)

veryclean:
	rm -rf $(BUILD_DIR)
