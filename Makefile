CC = gcc
CPPFLAGS += -Iinclude
CFLAGS ?= -O2 -g -Wall -Wextra
LDLIBS += -lm

TARGET = chs
ODIR = obj/
BIN_DIR = ./
BINARY = $(BIN_DIR)$(TARGET)
OBJ = $(addprefix $(ODIR),get_goals_rubi_csv.o chs.o chs_debug.o)
DEPS = $(wildcard include/*.h)

.PHONY: all test check clean
all: $(BINARY)

$(ODIR)%.o: src/%.c $(DEPS)
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(BINARY): $(OBJ)
	mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

# Retain the original functional sample as a quick smoke test.
test: $(BINARY)
	$(BINARY) -h 145 -p 5:30.0 test.csv

$(ODIR)test_calculations: tests/test_calculations.c $(ODIR)get_goals_rubi_csv.o $(ODIR)chs_debug.o $(DEPS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ tests/test_calculations.c $(ODIR)get_goals_rubi_csv.o $(ODIR)chs_debug.o $(LDLIBS)

check: $(BINARY) $(ODIR)test_calculations
	$(ODIR)test_calculations
	CHS_BINARY=$(abspath $(BINARY)) python3 tests/test_chs.py

clean:
	rm -f $(OBJ) $(BINARY) $(ODIR)test_calculations
