CFLAGS= -g 
OBJECTS= usb.o usbstring.o
LIBS = -lpthread

all: gadgetfs-test

gadgetfs-test: $(OBJECTS) 
	$(CC) -o gadgetfs-test  $(OBJECTS) $(LIBS)

clean:
	rm *.o
