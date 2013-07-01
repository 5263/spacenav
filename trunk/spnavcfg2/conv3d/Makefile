src = conv3d.c
obj = $(src:.c=.o)
bin = conv3d

CFLAGS = -pedantic -Wall -g -I../src
LDFLAGS = -lassimp

$(bin): $(obj)
	$(CC) -o $@ $(obj) $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(obj) $(bin)
