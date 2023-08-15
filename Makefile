TARGET := chatgpt
CC     := cc
CFLAGS := -Wall -O2
OUT    := .

PREFIX?=/usr/local

$(OUT)/%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

all: $(TARGET)

install:
	mkdir -p $(PREFIX)/bin
	install -c -m 555 $(TARGET) $(PREFIX)/bin

clean:
	rm -rf $(OUT)/*.o
	rm -rf $(TARGET)

OBJS = $(OUT)/main.o \
	   $(OUT)/json.o \
	   $(OUT)/http.o \
	   $(OUT)/aostr.o \
	   $(OUT)/list.o \
	   $(OUT)/dict.o \
	   $(OUT)/io.o \
	   $(OUT)/sql.o \
	   $(OUT)/cli.o \
	   $(OUT)/openai.o \
	   $(OUT)/linenoise.o \
	   $(OUT)/json-selector.o

$(TARGET): $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) -lcurl -lsqlite3

$(OUT)/main.o: \
	main.c \
	json-selector.h \
	json.h \
	linenoise.h \
	panic.h

$(OUT)/linenoise.o: \
	linenoise.c \
	linenoise.h

$(OUT)/openai.o: \
	openai.c \
	openai.h \
	aostr.h \
	json-selector.h \
	json.h \
	panic.h

$(OUT)/json.o: \
	./json.c \
	./json.h

$(OUT)/json-selector.o: \
	./json-selector.c \
	./json-selector.h \
	./json.h

$(OUT)/list.o: \
	./list.c \
	./list.h

$(OUT)/dict.o: \
	./dict.c \
	./dict.h

$(OUT)/io.o: \
	./io.c \
	./io.h

$(OUT)/cli.o: \
	./cli.c \
	./cli.h \
	./dict.h

$(OUT)/sql.o: \
	./sql.c \
	./sql.h

$(OUT)/http.o: \
	./http.c \
	./http.h \
	./json.h \
	./aostr.h \
	./panic.h

$(OUT)/aostr.o: \
	./aostr.c \
	./aostr.h \
	./panic.h
