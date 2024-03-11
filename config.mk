bdir = build
CC = clang
PREFIX = /usr/local
# default compile flags (overridable by environment)
CFLAGS ?= -g -Wall -Wextra -Werror -Wno-unused-parameter -Wno-sign-compare -Wno-unused-function -Wno-unused-variable -Wdeclaration-after-statement
# uncomment for xwayland support
CFLAGS += -DXWAYLAND

