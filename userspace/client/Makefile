# You may symlink ./deps/uv to libuv directory
CFLAGS= -g -I./deps/uv/include
all: libusbip-client.a libusb-usbip

libusbip-client.a: client.o
	$(AR) rcs $@ $^

libusb-usbip: 
	$(MAKE) -C libusb0 libusb-usbip.a

clean:
	rm *.a *.o
	$(MAKE) -C libusb0 clean
