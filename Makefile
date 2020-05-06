OBJS = dopewars.o

CC = m68k-palmos-coff-gcc

#uncomment this if you want to build a gdb debuggable version
#DEFINES = -DDEBUG

INCLUDES =

CSFLAGS = -O2 -S $(DEFINES) $(INCLUDES)
CFLAGS = -O2 -g $(DEFINES) $(INCLUDES)

PILRC = pilrc
TXT2BITM = txt2bitm
OBJRES = m68k-palmos-coff-obj-res
BUILDPRC = build-prc

ICONTEXT = "DopeWars"
APPID = Dope
PRC = dopewars.prc

all: $(PRC)

.S.o:
	$(CC) $(TARGETFLAGS) -c $<

.c.s:
	$(CC) $(CSFLAGS) $<

$(PRC): code.stamp bin.stamp
	$(BUILDPRC) $@ $(ICONTEXT) $(APPID) *.grc *.bin

code.stamp: dopewars
	$(OBJRES) dopewars
	touch code.stamp

bin.stamp: dopewars.rcp
	$(PILRC) dopewars.rcp
	touch bin.stamp

dopewars: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@

fullclean:
	rm -rf *.[oa] dopewars *.bin *.stamp *.grc dopewars.prc

clean:
	rm -rf *.[oa] dopewars *.bin *.stamp *.grc

