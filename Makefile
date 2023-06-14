CC = cc
CFLAGS = -Wall -O2
LDFLAGS =

OBJS = vi.o ex.o lbuf.o mot.o sbuf.o ren.o dir.o syn.o reg.o led.o \
	uc.o term.o rset.o rstr.o regex.o cmd.o tag.o conf.o
STAG = stag.o regex.o

all: vi

cmd.o: cmd.c vi.h
conf.o: conf.c vi.h conf.h kmap.h
dir.o: dir.c vi.h
ex.o: ex.c vi.h
lbuf.o: lbuf.c vi.h
led.o: led.c vi.h
mot.o: mot.c vi.h
reg.o: reg.c vi.h
regex.o: regex.c regex.h
ren.o: ren.c vi.h
rset.o: rset.c regex.h vi.h
rstr.o: rstr.c vi.h
sbuf.o: sbuf.c vi.h
stag.o: stag.c regex.h
syn.o: syn.c vi.h
tag.o: tag.c vi.h
term.o: term.c vi.h
uc.o: uc.c vi.h
vi.o: vi.c vi.h

%.o: %.c
	$(CC) -c $(CFLAGS) $<
vi: $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)
stag: $(STAG)
	$(CC) -o $@ $(STAG) $(LDFLAGS)
clean:
	rm -f *.o vi stag
