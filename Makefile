CFLAGS=-Os -Wall -pedantic
BINS=discjockey disctype discwait

.c:
	$(CC) $(CFLAGS) -o $@ $< && strip $@

all: $(BINS)

clean:
	-rm -f $(BINS)

discjockey: discjockey.c

disctype: disctype.c

discwait: discwait.c
