CC      = gcc
CFLAGS  = -Wall -Wextra -g -O0 -std=gnu11 -Wno-unused-parameter
STUBS   = $(CURDIR)/stubs
CORE    = $(CURDIR)/..

# Search stubs FIRST so mock headers take priority
CFLAGS += -I$(STUBS)
# Then the fabric-core root so that #include "./fabric/..." etc. resolve
CFLAGS += -I$(CORE)

SRCS = $(CORE)/main.c                \
       $(CORE)/fabric/fabric.c       \
       $(CORE)/controller/controller.c \
       $(CORE)/bridge/bridge.c        \
       $(CORE)/component/component.c  \
       $(CORE)/map/map.c              \
       $(CORE)/message/message.c      \
       $(CORE)/arbiter/arbiter.c      \
       $(CORE)/queue/queue.c          \
       $(CORE)/include/isolation.c    \
       $(CORE)/backends/backends.c    \
       $(CORE)/backends/mpk_backend.c

LDFLAGS = -lpthread
TARGET  = fabric_demo

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)