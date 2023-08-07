TARGET := chat
CC     := cc
CFLAGS := -Wall -O2
OUT    := .

$(OUT)/%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

all: $(TARGET)

clean:
	rm -rf $(OUT)/*.o
	rm -rf $(TARGET)

OBJS = $(OUT)/main.o \
	   $(OUT)/json.o \
	   $(OUT)/http.o \
	   $(OUT)/aostr.o \
	   $(OUT)/list.o \
	   $(OUT)/io.o \
	   $(OUT)/sql.o \
	   $(OUT)/prompt.o \
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

$(OUT)/io.o: \
	./io.c \
	./io.h

$(OUT)/prompt.o: \
	./prompt.c \
	./prompt.h

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
