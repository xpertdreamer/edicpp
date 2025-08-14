// term.hpp
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
#include <sys/ioctl.h>
#include <string.h>
#include <string>

class Term {
public:
    Term();
    ~Term();

    bool editorProccessKeypress();
    void editorRefreshScreen();
    void initEditor();
    
private:
    struct OrigTermCfg {
        struct termios orig_term;
        int screen_rows;
        int screen_cols;
    } config_;

    std::string abuf;

    void enableRawMode();
    void disableRawMode();
    void die(const char *msg);
    char editorReadKey();
    void editorDrawRows(std::string &ab);
    int getWindowSize(int *rows, int *cols);
    int getCursorPosition(int *rows, int *cols);
};

#endif // TERM_HPP