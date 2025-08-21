// term.hpp
#pragma once
#ifndef TERM_HPP
#define TERM_HPP

#include <cctype>
#include <iostream>
#include <string>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <stdarg.h>
#include <fcntl.h>

class Term {
public:
    Term();
    ~Term();

    bool editorProccessKeypress();
    void editorRefreshScreen();
    void initEditor();
    void editorOpen(char *filename);
    void editorSetStatusMessage(const char *fmt, ...);
    
private:
    typedef struct TextRow {
        int size;
        int r_size;
        char *chars;
        char *render;
    } trow_;

    struct OrigTermCfg {
        struct termios orig_term;
        int screen_rows;
        int screen_cols;
        int cursor_x;
        int cursor_y;
        int r_x;
        int row_offset;
        int col_offset;
        int numrows;
        trow_ *row;
        char *filename;
        char statusMsg[80];
        time_t statusMsg_time;
        int dirty;
    } CFG;

    std::string abuf;

    enum editorKey {
        BACKSPACE = 127,
        ARROW_LEFT = 1000,
        ARROW_RIGHT,
        ARROW_UP,
        ARROW_DOWN,
        CTRL_ARROW_LEFT,
        CTRL_ARROW_RIGHT,
        CTRL_ARROW_UP,
        CTRL_ARROW_DOWN,
        DEL_KEY
    };

    void editorMoveCursor(int key);
    void enableRawMode();
    void disableRawMode();
    void die(const char *msg);
    int editorReadKey();
    void editorDrawRows(std::string &ab);
    int getWindowSize(int *rows, int *cols);
    int getCursorPosition(int *rows, int *cols);
    void editorInsertRow(int at, char *s, size_t len);
    void editorScroll();
    void editorUpdateRow(trow_ *row);
    int editorRowCxToRx(trow_ *row, int cx);
    void editorDrawStatusBar(std::string &ab);
    void editorDrawMessageBar(std::string &ab);
    void editorRowInsertChar(trow_ *row, int at, int c);
    void editorInsertChar(int c);
    char *editorRowToString(int *buflen);
    void editorSave();
    void editorRowDeleteChar(trow_ *row, int at);
    void editorDelChar();
    void editorFreeRow(trow_ *row);
    void editorDelRow(int at);
    void editorRowAppendString(trow_ *row, char *s, size_t len);
    void editorInsertNewLine();
    char *editorPrompt(char *prompt);
};

#endif // TERM_HPP