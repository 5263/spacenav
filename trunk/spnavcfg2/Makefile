PREFIX = /usr/local

src = $(wildcard src/*.c)
obj = $(src:.c=.o)
dep = $(obj:.o=.d)
bin = spnavcfg2

incpaths = -I/usr/local/include -I/usr/X11R6/include
libpaths = -L/usr/local/lib -L/usr/X11R6/lib

opt = -O3 -ffast-math

CFLAGS = -std=gnu89 -pedantic -Wall -g $(opt) $(incpaths)
LDFLAGS = $(libpaths) $(libgl) -lspnav -lpthread

sys = $(shell uname -s)
ifeq ($(sys), Darwin)
	libgl = -framework OpenGL -framework GLUT -lGLEW
else
	libgl = -lGL -lGLU -lglut -lGLEW
endif

ifeq ($(sys), $(filter $(sys), IRIX IRIX64))
	CC = gcc
endif

$(bin): $(obj)
	$(CC) -o $@ $(obj) $(LDFLAGS)

-include $(dep)

%.d: %.c
	@$(CPP) $(CFLAGS) $< -MM -MT $(@:.d=.o) >$@

.PHONY: clean
clean:
	rm -f $(obj) $(bin)
