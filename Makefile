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

# --------------- Kurallar ---------------

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

$(SRCDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TEST_TARGET): $(TEST_SRCS)
	$(CC) $(CFLAGS) -o $(TEST_TARGET) $(TEST_SRCS)

test: $(TEST_TARGET)
	./$(TEST_TARGET)

clean:
ifeq ($(OS),Windows_NT)
	-del /Q $(SRCDIR)\*.o $(TARGET).exe $(TEST_TARGET).exe 2>NUL
else
	rm -f $(SRCDIR)/*.o $(TARGET) $(TEST_TARGET) $(TARGET).exe $(TEST_TARGET).exe
endif

.PHONY: all test clean
