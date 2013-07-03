PREFIX = /usr/local

src = $(wildcard src/*.c)
obj = $(src:.c=.o)
dep = $(obj:.o=.d)
bin = spnavcfg2

incpaths = -I/usr/local/include -I/usr/X11R6/include
libpaths = -L/usr/local/lib -L/usr/X11R6/lib

CFLAGS = -std=gnu99 -pedantic -Wall -g $(incpaths)
LDFLAGS = $(libpaths) $(libgl) -lspnav

ifeq ($(shell uname -s), Darwin)
	libgl = -framework OpenGL -framework GLUT -lGLEW
else
	libgl = -lGL -lGLU -lglut -lGLEW
endif

$(bin): $(obj)
	$(CC) -o $@ $(obj) $(LDFLAGS)

-include $(dep)

%.d: %.c
	@$(CPP) $(CFLAGS) $< -MM -MT $(@:.d=.o) >$@

.PHONY: clean
clean:
	rm -f $(obj) $(bin)
