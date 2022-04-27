#pragma once

#include <vld.h>

#include <stdio.h>
#include <tchar.h>
#include <conio.h>

#ifdef WIN32
#	define WIN32_LEAN_AND_MEAN
#	include <Windows.h>

#	include <winsock2.h>
#	include <ws2tcpip.h>

#	pragma comment (lib, "Ws2_32.lib")
#endif
