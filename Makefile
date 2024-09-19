include maketh/target/build.rules

CXXFLAGS += -std=c++17
LDFLAGS += -lstdc++

ifeq ($(TARGET_OS),linux)
ELFYN_LINUX = 1
ELFYN_EPOLL ?= 1
else
$(error OS not supported [yet])
endif

$(eval $(call add-define,ELFYN_LINUX))
$(eval $(call add-define,ELFYN_EPOLL))

# elfyn
elfyn-src += src
ifeq ($(ELFYN_EPOLL),1)
elfyn-src += source/epoll
endif
elfyn-inc += include
$(eval $(call add-lib,elfyn))

# test
test-core-cpp += tests/test-core.cpp
test-core-lib += elfyn
$(eval $(call add-app,test-core))

$(eval $(build))
