CC:=clang
LDFLAGS+=-pthread
OBJS=$(patsubst %.c,%.o,$(wildcard *.c))

TARGET=program
# executable file is "program"
# run with ./program path_to_device

bin: $(TARGET)

$(OBJS): %.o: %.c
	$(CC) -c $< $(CFLAGS) $(CPPFLAGS) -o $@

$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

clean:
	$(RM) $(OBJS) $(TARGET)
