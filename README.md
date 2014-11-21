funterm
=======

FUNterm is a serial-port terminal program written for windows.  It is written using only Win32 calls, using no special libraries.  As a consequence, it runs cleanly with Wine, making it a useful tool for talking to serial devices under Linux as well as Windows.  In fact, most of its use and development has been with Linux, though it has had extensive use under Windows as well.

The project is complete and ready for use as a terminal, and is quite simple.  It compiles with MinGW using the supplied Makefile.

One of the modules included is a stand-alone serial-port driver for Windows.  This can be used to add serial connectivity to any open-source Win32 project, with no added library requirements.

The program has been completely documented with Doxygen.
