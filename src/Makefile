#
# Dreamcast Flashrom Tool Makefile
#

TARGET = flashtool.elf
OBJS = fb_console.o utils.o flashrom.o flashtool.o

all: $(TARGET)

$(TARGET): $(OBJS)
	kos-cc -o $(TARGET) $(OBJS)

clean:
	-rm -f $(OBJS) $(TARGET)

.PHONY: clean all

include $(KOS_BASE)/Makefile.rules
