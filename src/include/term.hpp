#pragma once
#ifndef TERM_HPP
#define TERM_HPP

#include <cctype>
#include <iostream>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>

class Term {
    public:
        Term();
        ~Term();
        void readInput();
    private:
        static struct termios orig_term;
        void enableRawMode();
        void disableRawMode();
        static void disableRawModeStatic();
        void die(const char *msg);
};

#endif //TERM_HPP
