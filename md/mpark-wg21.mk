THIS_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
DEFAULTS := $(THIS_DIR)defaults.yaml
include $(THIS_DIR)wg21/paper.mk
