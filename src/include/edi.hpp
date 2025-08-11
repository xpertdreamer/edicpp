#ifndef EDI_HPP
#define EDI_HPP

#include <cctype>
#include <iostream>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>

void enableRawMode();
void disableRawMode();
void die(const char *s);

#endif
