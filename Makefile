CC = cc
CFLAGS = -Wall -O2
LDFLAGS =

OBJS = vi.o ex.o lbuf.o mot.o sbuf.o ren.o dir.o syn.o reg.o led.o \
	uc.o term.o rset.o rstr.o regex.o cmd.o tag.o conf.o
STAG = stag.o regex.o

all: vi

conf.o: conf.h

%.o: %.c
	$(CC) -c $(CFLAGS) $<
vi: $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)
stag: $(STAG)
	$(CC) -o $@ $(STAG) $(LDFLAGS)
clean:
	rm -f *.o vi stag tags
