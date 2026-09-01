#pragma once
#define _CRT_SECURE_NO_WARNINGS

// Verhindere, dass winsock.h (Version 1) eingebunden wird, da wir winsock2.h brauchen
#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_
#endif

#include <string>
#include <iostream>
