LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := map_experiment
LOCAL_CFLAGS += -g -O0
LOCAL_SRC_FILES := main.cpp
LOCAL_LDLIBS :=

include $(BUILD_EXECUTABLE)