include ../Makefile.header

LDFLAGS	+= -Ttext 0

all: bootsect setup

bootsect: bootsect.s
	@$(AS) -g -o bootsect.o bootsect.s
	@$(LD) $(LDFLAGS) -o bootsect bootsect.o
	@cp bootsect ../tools/bootsect
	@$(OBJCOPY) -R .pdr -R .comment -R.note -S -O binary bootsect


setup: setup.s
	@$(AS) -g -o setup.o setup.s
	@$(LD) $(LDFLAGS) -o setup setup.o
	@cp setup ../tools/setup
	@$(OBJCOPY) -R .pdr -R .comment -R.note -S -O binary setup

head.o: head.s
	@$(AS) -g -o head.o head.s

clean:
	@rm -f bootsect bootsect.o setup setup.o head.o
