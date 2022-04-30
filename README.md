# ftp_server
simple esp32\windows\linux ftp server implementation

Where is no arduino library. It's standalone multiplatform code of ftp-server.

transfer speed ~220kb/sec

Limitations:
- no multi-session suport
- binary mode only
- no auth (any login succeed)
- not all ftp commands are implemented
- server messages callback api (where are many debug messages outputs via printf)
- correct stop of the ftp server (laziness)

There are several bugs in code i know - passive mode is need to fix, but overall code are working.

Compile:

You need to include 'ftp_server.h' from src. And add all *.cpp files in your project.

Windows\Linux\ESP32(esp-idf) examples are contains in 'solutions' folder.

--------

History:
2022-04-30. ...
