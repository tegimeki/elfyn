include maketh/target/build.rules

CXXFLAGS += -std=c++17
LDFLAGS += -lstdc++

ifeq ($(TARGET_OS),linux)
ELFYN_LINUX = 1
ELFYN_EPOLL ?= 1
ELFYN_STRING ?= 1
ELFYN_TCP ?= 1
else
$(error OS not supported [yet])
endif

$(eval $(call add-define,ELFYN_LINUX))
$(eval $(call add-define,ELFYN_EPOLL))
$(eval $(call add-define,ELFYN_TCP))
$(eval $(call add-define,ELFYN_STRING))

# elfyn
elfyn-src += src
elfyn-inc += include
ifeq ($(ELFYN_EPOLL),1)
elfyn-src += source/epoll
endif
ifeq ($(ELFYN_TCP),1)
elfyn-src += source/tcp
endif
$(eval $(call add-lib,elfyn))

# test
test-core-cpp += tests/test-core.cpp
test-core-lib += elfyn
$(eval $(call add-app,test-core))

test-tcp-server-cpp += tests/test-tcp-server.cpp
test-tcp-server-lib += elfyn
$(eval $(call add-app,test-tcp-server))

$(eval $(build))
