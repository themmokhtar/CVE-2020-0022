USE_VALGRIND ?= false

BUILDDIR = build
SOURCEDIR = src
INCLUDEDIR = include

INCLUDES = -Iinclude -Iinclude/**
LIBRARIES = -lbluetooth

ifeq ($(USE_VALGRIND),true)
COMPILE = $(CC) $(CFLAGS) $(INCLUDES) -g -O0 -Wall -Werror
else
COMPILE = $(CC) $(CFLAGS) $(INCLUDES) -Wall -Werror
endif

DEPS = $(wildcard $(SOURCEDIR)/*.c)
OBJS = $(patsubst $(SOURCEDIR)/%.c,$(BUILDDIR)/%.o,$(DEPS))
HEADERS = $(wildcard $(INCLUDEDIR)/*.h)

MAP_EXPERIMENT_DIR = $(notdir $(shell pwd)/map_experiment)
JOP_EXPERIMENT_DIR = $(notdir $(shell pwd)/jop_experiment)

.PHONY: build run clean build-map-experiment run-map-experiment map-experiment clean-map-experiment build-jop-experiment run-jop-experiment jop-experiment clean-jop-experiment

build: $(BUILDDIR)/exploit.out

$(BUILDDIR)/exploit.out: $(OBJS)
	$(COMPILE) -o $@ $^ $(LIBRARIES)

$(BUILDDIR)/%.o: $(SOURCEDIR)/%.c $(HEADERS)
	$(COMPILE) -c -o $@ $< $(LIBRARIES)

run: build
	sudo systemctl restart bluetooth.service
	sleep 1.5

	@printf "\nRunning exploit...\n\n"

ifeq ($(USE_VALGRIND),true)
		sudo valgrind --leak-check=full ./exploit $(ARGS)
else
		sudo ./exploit $(ARGS)
endif

build-map-experiment:
	cd $(MAP_EXPERIMENT_DIR) && NDK_PROJECT_PATH=. ndk-build NDK_APPLICATION_MK=./Application.mk

run-map-experiment: build-map-experiment
	@printf "\nPushing map_experiment...\n"
	adb push $(MAP_EXPERIMENT_DIR)/obj/local/arm64-v8a/map_experiment /data/local/tmp/map_experiment
	adb shell chmod +x /data/local/tmp/map_experiment

	@printf "\nRunning map_experiment...\n\n"
	adb shell /data/local/tmp/map_experiment

map-experiment: build-map-experiment run-map-experiment

clean-map-experiment:
	cd $(MAP_EXPERIMENT_DIR) && NDK_PROJECT_PATH=. ndk-build clean NDK_APPLICATION_MK=./Application.mk

build-jop-experiment:
	cd $(JOP_EXPERIMENT_DIR) && NDK_PROJECT_PATH=. ndk-build NDK_APPLICATION_MK=./Application.mk

run-jop-experiment: build-jop-experiment
	@printf "\nPushing jop_experiment...\n"
	adb push $(JOP_EXPERIMENT_DIR)/obj/local/arm64-v8a/jop_experiment /data/local/tmp/jop_experiment
	adb shell chmod +x /data/local/tmp/jop_experiment

	@printf "\nRunning jop_experiment...\n\n"
	adb shell /data/local/tmp/jop_experiment

jop-experiment: build-jop-experiment run-jop-experiment

clean-jop-experiment:
	cd $(JOP_EXPERIMENT_DIR) && NDK_PROJECT_PATH=. ndk-build clean NDK_APPLICATION_MK=./Application.mk

clean:
	rm -f $(BUILDDIR)/*.o $(BUILDDIR)/exploit.out

