src = conv3d.c
obj = $(src:.c=.o)
bin = conv3d

CFLAGS = -pedantic -Wall -g -I../src
LDFLAGS = $(libgl) -lassimp

ifeq ($(shell uname -s), Darwin)
	libgl = -framework OpenGL -framework GLUT -lglew
else
	libgl = -lGL -lGLU -lglut -lglew
endif

$(bin): $(obj)
	$(CC) -o $@ $(obj) $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(obj) $(bin)
