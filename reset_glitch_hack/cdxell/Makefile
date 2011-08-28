CROSS=xenon-
CC=$(CROSS)gcc
OBJCOPY=$(CROSS)objcopy
LD=$(CROSS)ld
AS=$(CROSS)as
STRIP=$(CROSS)strip

# Configuration
AFLAGS = -m64

# Build rules
all: CD CDjasper

clean:
	rm -rf cdxell.o cdxell.bin CD CDjasper

.S.o:
	$(CC) $(AFLAGS) -c -o $@ $*.S

%.bin: %.o
	$(OBJCOPY) -O binary $< $@

CD: cdxell.bin
	cp CD.tpl CD
	dd bs=624 seek=1 if=cdxell.bin of=CD conv=notrunc

CDjasper: cdxell.bin
	cp CDjasper.tpl CDjasper
	dd bs=624 seek=1 if=cdxell.bin of=CDjasper conv=notrunc