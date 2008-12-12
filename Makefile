FRAMEWORKS=-framework CoreFoundation -framework IOKit

all:
	gcc -dynamiclib usbbox_macosx.c -fvisibility=hidden $(FRAMEWORKS) -o libusbbox.dylib

clean:
	rm libusbbox.dylib
	rm *.o
	rm test_usbbox

demo:
	gcc $(FRAMEWORKS) usbbox_macosx.c usbbox_demo.c -o usbbox

compile_test:
	gcc $(FRAMEWORKS) usbbox_macosx.c test_usbbox.c -o test_usbbox

test: compile_test
	./test_usbbox