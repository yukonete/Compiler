CONFIG ?= debug

CXXFLAGS = -std=c++23 -Wall -Wextra -fhardened -pedantic -fno-exceptions -Wsign-conversion
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
SRCS := $(SRC_DIR)base/arena.cpp \
		$(SRC_DIR)base/file.cpp \
		$(SRC_DIR)base/utf8.cpp \
		$(SRC_DIR)ast.cpp \
		$(SRC_DIR)error.cpp \
		$(SRC_DIR)lexer.cpp \
		$(SRC_DIR)parser.cpp \
		$(SRC_DIR)typer.cpp \
		$(SRC_DIR)utf8proc/utf8proc.c \
		$(SRC_DIR)terminal_linux.cpp \
		$(SRC_DIR)main.cpp

CPP_SRCS := $(filter %.cpp,$(SRCS))
C_SRCS := $(filter %.c,$(SRCS))
OBJS := $(addprefix $(OBJDIR), $(CPP_SRCS:.cpp=.o) $(C_SRCS:.c=.o))
DEPS := $(OBJS:.o=.d)

TARGET_DIR = $(CONFIGURATION_DIR)bin/
TARGET = $(TARGET_DIR)compiler

.PHONY: build

build: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(TARGET) $(LINK)

$(OBJDIR)%.o : %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c $< -o $@ -MMD -MP 

$(OBJDIR)%.o : %.c
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c $< -o $@ -MMD -MP 

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
