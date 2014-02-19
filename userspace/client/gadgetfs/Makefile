CFLAGS= -g 
OBJECTS= gadgetfs.o
LIBS = -L../deps/uv/ -L../ -Wl,-Bstatic -lusbip-client -luv -Wl,-Bdynamic -lpthread -ldl -lrt

all: gadgetfs

libusbip-client:
	$(MAKE) -C ../ libusbip-client.a

gadgetfs: libusbip-client $(OBJECTS) 
	$(CC) -o gadgetfs  $(OBJECTS) $(LIBS)

clean:
	rm *.a *.o
