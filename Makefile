CONFIG ?= debug

CXXFLAGS = -std=c++23 -Wall -Wextra -fhardened -pedantic -MMD -MP -fno-rtti -fno-exceptions
INCLUDE = -Icode/
LINK=-lstdc++exp

BUILD_DIR = build/
SRC_DIR = code/

ifeq ($(CONFIG), debug)
    CXXFLAGS += -g -O0 -fsanitize=address -Wno-hardened
    CONFIGURATION_DIR = $(BUILD_DIR)debug/
else ifeq ($(CONFIG), release)
    CXXFLAGS += -O3
    CONFIGURATION_DIR = $(BUILD_DIR)release/
else
    $(error Unknown configuration: $(CONFIG))
endif

OBJDIR = $(CONFIGURATION_DIR)obj/
SRCS := $(shell find $(SRC_DIR) -name "*.cpp")
OBJS := $(addprefix $(OBJDIR), $(SRCS:.cpp=.o))
DEPS := $(OBJS:.o=.d)

TARGET_DIR = $(CONFIGURATION_DIR)bin/
TARGET = $(TARGET_DIR)compiler

.PHONY: debug

debug: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(TARGET) $(LINK)

$(OBJDIR)%.o : %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c $< -o $@

-include $(DEPS)

.PHONY: clean rebuild

rebuild: clean
	$(MAKE) $(TARGET)

clean :
	-rm $(OBJDIR) -r
	-rm $(TARGET_DIR) -r


$(TARGET): | $(TARGET_DIR)

$(TARGET_DIR):
	-mkdir -p $(TARGET_DIR)


$(OBJS): | $(OBJDIR)

$(OBJDIR):
	-mkdir -p $(OBJDIR)
