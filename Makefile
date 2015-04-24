CFLAGS += -Wall -Wextra -Wno-unused-parameter -g
LIBS += $(shell pkg-config --libs wayland-client wayland-cursor)
INCLUDES += $(shell pkg-config --cflags wayland-client wayland-cursor)
OBJS = os-compatibility.o cursor-viewer.o
TARGET = cursor-viewer

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

clean:
	rm -f $(OBJS) $(TARGET)
