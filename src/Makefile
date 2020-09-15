CC		= gcc 
CFLAGS	= -g
INCS	= -I./confparser/
LDS		= -luuid

SHLD	= $(CC) $(CFLAGS)

all:	sn-send sn-inq sn-start sn-rspawn sn-remote

sn-send:	send.o trigger.o sig.o prioq.o alloc.o slog.o stralloc.o \
		utils.o \
		confparser/libconfparser.a
	$(SHLD) -o sn-send send.o trigger.o sig.o prioq.o alloc.o slog.o \
		stralloc.o utils.o \
		confparser/libconfparser.a \
		$(INCS) $(LDS)

sn-inq:	inq.o trigger.o utils.o slog.o
	$(SHLD) -o sn-inq inq.o trigger.o utils.o slog.o \
		$(INCS) $(LDS)

sn-start:	start.o utils.o
	$(SHLD) -o sn-start start.o utils.o $(INCS) $(LDS)

sn-rspawn: rspawn.o utils.o stralloc.o slog.o alloc.o sig.o \
	confparser/libconfparser.a
	$(SHLD) -o sn-rspawn rspawn.o utils.o stralloc.o slog.o \
	alloc.o sig.o \
	confparser/libconfparser.a $(INCS) $(LDS)

sn-remote: remote.o slog.o \
	confparser/libconfparser.a
	$(SHLD) -o sn-remote remote.o slog.o \
	confparser/libconfparser.a $(INCS) $(LDS)

send.o:	send.c
	$(SHLD) -c -o send.o send.c $(INCS) 

inq.o: inq.c
	$(SHLD) -c -o inq.o inq.c $(INCS) 

start.o: start.c
	$(SHLD) -c -o start.o start.c $(INCS) 

rspawn.o: rspawn.c
	$(SHLD) -c -o rspawn.o rspawn.c $(INCS)

remote.o: remote.c
	$(SHLD) -c -o remote.o remote.c $(INCS)

trigger.o: trigger.c
	$(SHLD) -c -o trigger.o trigger.c $(INCS) 

sig.o: sig.c
	$(SHLD) -c -o sig.o sig.c $(INCS) 

prioq.o: prioq.c
	$(SHLD) -c -o prioq.o prioq.c $(INCS) 

slog.o: slog.c
	$(SHLD) -c -o slog.o slog.c $(INCS) 

alloc.o: alloc.c
	$(SHLD) -c -o alloc.o alloc.c $(INCS) 

utils.o: utils.c
	$(SHLD) -c -o utils.o utils.c $(INCS) 

stralloc.o: stralloc.c
	$(SHLD) -c -o stralloc.o stralloc.c $(INCS) 

confparser/libconfparser.a:
	cd confparser; make -f linux.mk; cd -


install:
	echo "Prefix="$(prefix)
	test -d $(prefix)/bin || mkdir -p $(prefix)/bin
	test -d $(prefix)/conf || mkdir -p $(prefix)/conf
	cp sn-inq $(prefix)/bin/
	cp sn-start $(prefix)/bin/
	cp sn-send $(prefix)/bin/
	cp sn-rspawn $(prefix)/bin/
	cp sn-remote $(prefix)/bin/
	cp snotify.sh $(prefix)/bin/
	cp conf/* $(prefix)/conf/


clean:
	rm -f *.o
	rm -f sn-send
	rm -f sn-inq
	rm -f sn-start
	rm -f sn-rspawn
	rm -f sn-remote
