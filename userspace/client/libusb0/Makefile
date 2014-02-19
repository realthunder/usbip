CFLAGS= -g 
OBJECTS= usb.o usbip.o error.o descriptors.o

all: libusb-usbip.a

libusb-usbip.a: $(OBJECTS)
	$(AR) rcs $@ $^

clean:
	rm *.a *.o
