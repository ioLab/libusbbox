Makefile, code for library, unit tests and demo code included.

Files:

* usbbox.h - the main header file and the library interface
* usbbox_macosx.c - the mac os x implementation of the library
* test_usbbox.c - simple units tests for the library (basic read/writes etc)
* usbbox_demo.c - demo program (crude CLI that reads/writes on stdin/stdout)
* Makefile - the make file

Makefile commands:

* make all - will create the dynamic library 'libusbbox.dylib'.
* make test - will compile and run unit tests (needs box to be plugged in)
* make demo - create the demo application (usbbox executable)

The Demo app:

* ./usbbox - run the demo app

As it reads/writes on stdin/stdout it should be possible to use it as part of a 
shell script.

e.g.

> echo '0x52,0x00,0x00,0x00,0x00,0x00,0x00,0x00' | ./usbbox

Send a clock reset command to the box.

> cat command.txt | ./usbbox

Could be used to run a set of commands into the box (one per line).

> ./usbbox | grep '0x55,.*' > keys.txt

Would get the keys that the user pressed. etc.

With appropriate use of name pipes etc it would be more or less possible
to right an full app using just this CLI interface.
