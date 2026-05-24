# ============================================================
# tarsau - Arsivleme Programi Makefile
# ============================================================

CC       = gcc
CFLAGS   = -Wall -Wextra -std=c11 -Iinclude
TARGET   = tarsau
TEST_TARGET = test_tarsau

SRCDIR   = src
SRCS     = $(SRCDIR)/main.c \
           $(SRCDIR)/archive.c \
           $(SRCDIR)/extract.c \
           $(SRCDIR)/utils.c
OBJS     = $(SRCS:.c=.o)
TEST_SRCS = test/test_tarsau.c \
            $(SRCDIR)/archive.c \
            $(SRCDIR)/extract.c \
            $(SRCDIR)/utils.c

ifeq ($(OS),Windows_NT)
RM_OBJECTS = -del /Q $(SRCDIR)\*.o 2>NUL
RM_BINARIES = -del /Q $(TARGET).exe $(TEST_TARGET).exe 2>NUL
else
RM_OBJECTS = rm -f $(SRCDIR)/*.o
RM_BINARIES = rm -f $(TARGET) $(TEST_TARGET) $(TARGET).exe $(TEST_TARGET).exe
endif

# --------------- Kurallar ---------------

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)
	$(RM_OBJECTS)

$(SRCDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TEST_TARGET): $(TEST_SRCS)
	$(CC) $(CFLAGS) -o $(TEST_TARGET) $(TEST_SRCS)

test: $(TEST_TARGET)
	./$(TEST_TARGET)

clean:
	$(RM_OBJECTS)
	$(RM_BINARIES)

.PHONY: all test clean
