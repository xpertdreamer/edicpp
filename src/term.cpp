/*** defines ***/
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE
#define CTRL_KEY(key) ((key) & 0x1f)
#define TAB_STOP 8

/*** includes ***/
#include "include/term.hpp"
#include "version.hpp"

/*** usings ***/
using namespace std;

/*** init ***/
Term::Term() : config_ {}{
    enableRawMode();
}

Term::~Term() {
    disableRawMode();
}

/*** methods ***/
void Term::disableRawMode() {
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &config_.orig_term) == -1) {
        die("tcsetattr in disableRawMode");
    }

    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
}

void Term::enableRawMode() {
    if (tcgetattr(STDIN_FILENO, &config_.orig_term) == -1) {
        die("tcgetattr");
    }
    
    struct termios raw = config_.orig_term;
    raw.c_iflag &= ~(BRKINT | INPCK | ISTRIP | IXON | ICRNL);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        die("tcsetattr");
    }
}

void Term::die(const char *msg) {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);

    perror(msg);
    exit(1);
}

int Term::editorReadKey() {
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) die("read");
    }

    if (c == '\x1b') {
        char seq[8];

        if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

        if (seq[0] == '[') {
            if (seq[1] == '1') {
                if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
                if (read(STDIN_FILENO, &seq[3], 1) != 1) return '\x1b';
                if (seq[2] == ';' && seq[3] == '5') {
                    if (read(STDIN_FILENO, &seq[4], 1) != 1) return '\x1b';
                    switch (seq[4]) {
                    case 'A':
                        return CTRL_ARROW_UP;
                        break;
                    case 'B':
                        return CTRL_ARROW_DOWN;
                        break;
                    case 'C':
                        return CTRL_ARROW_RIGHT;
                    case 'D':
                        return CTRL_ARROW_LEFT;
                    }
                }
            } else if (seq[1] >= 0 && seq [1] <= 9) {
                if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
                if (seq[2] == '~') {
                    switch(seq[1]) {
                        /* implement other keys */
                        case '3': return DEL_KEY;
                    }
                }
            } else {
                switch (seq[1]) {
                    case 'A': return ARROW_UP;
                    case 'B': return ARROW_DOWN;
                    case 'C': return ARROW_RIGHT;
                    case 'D': return ARROW_LEFT;
                }
            }
        }

        return '\x1b';
    } else {
        return c;
    }
}

bool Term::editorProccessKeypress() {
    int c = editorReadKey();

    switch(c) {
        case CTRL_KEY('q'):
            return false;
        case CTRL_ARROW_UP:
            // fall through
        case CTRL_ARROW_DOWN:
        {
            if (c == CTRL_ARROW_UP) config_.cursor_y = config_.row_offset;
            else if (c == CTRL_ARROW_DOWN) {
                config_.cursor_y = config_.row_offset + config_.screen_cols - 1;
                if (config_.cursor_y > config_.numrows) config_.cursor_y = config_.numrows;
            }

            int times = config_.screen_rows;
            while (times--) {
                editorMoveCursor(c == CTRL_ARROW_UP ? ARROW_UP : ARROW_DOWN);
            }
            break;
        }
        case CTRL_ARROW_RIGHT:
            if (config_.cursor_y < config_.numrows) config_.cursor_x = config_.row[config_.cursor_y].size;
            break;
        case CTRL_ARROW_LEFT:
            // fall through
        case ARROW_UP: 
            // fall through
        case ARROW_DOWN:
            // fall through
        case ARROW_LEFT:
            // fall through
        case ARROW_RIGHT:
            editorMoveCursor(c);
            break;
    }
    return true;
}

void Term::editorRefreshScreen() {
    editorScroll();

    string ab;

    ab += "\x1b[?25l";
    ab += "\x1b[H";
    
    editorDrawRows(ab);
    editorDrawStatusBar(ab);
    editorDrawMessageBar(ab);

    char buffer[32];
    snprintf(buffer, sizeof(buffer), "\x1b[%d;%dH", (config_.cursor_y - config_.row_offset) + 1, (config_.r_x - config_.col_offset) + 1);
    ab += buffer;

    ab += "\x1b[?25h";

    write(STDOUT_FILENO, ab.c_str(), ab.length());
}

void Term::editorDrawRows(string &ab) {
    for (int y = 0; y < config_.screen_rows; y++) {
        int fileRow = y + config_.row_offset;
        if (fileRow >= config_.numrows) {
            if (config_.numrows == 0 && y == config_.screen_rows / 2) {
                char welcome_msg[80];
                int welcomelen = snprintf(welcome_msg, sizeof(welcome_msg),
                "%s -- version %s", 
                PROJECT_NAME, 
                PROJECT_VERSION);

            if (welcomelen > config_.screen_cols) welcomelen = config_.screen_cols;
            int padding = (config_.screen_cols - welcomelen) / 2;
            if (padding) {
                ab += "~";
                padding--;
            }
            while (padding--) ab += " ";

            ab += welcome_msg;
            } else {
            ab += '~';
            }
        } else {
            int len = config_.row[fileRow].r_size - config_.col_offset;
            if (len < 0) len = 0;
            if (len > config_.screen_cols) len = config_.screen_cols;
            ab.append(&config_.row[fileRow].render[config_.col_offset], len);
        }
            ab += "\x1b[K";
            ab += "\r\n";
    }
}

void Term::editorScroll() {
    config_.r_x = 0;
    if (config_.cursor_y < config_.numrows) config_.r_x = editorRowCxToRx(&config_.row[config_.cursor_y], config_.cursor_x);

    if (config_.cursor_y < config_.row_offset) config_.row_offset = config_.cursor_y;
    if (config_.cursor_y >= config_.row_offset + config_.screen_rows) config_.row_offset = config_.cursor_y - config_.screen_rows + 1;
    if (config_.r_x < config_.col_offset) config_.col_offset = config_.r_x;
    if (config_.r_x >= config_.col_offset + config_.screen_cols) config_.col_offset = config_.r_x - config_.screen_cols + 1;
}

int Term::getWindowSize(int *rows, int *cols) {
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
        editorReadKey();
        return -1;
    } else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}

int Term::getCursorPosition(int *rows, int *cols) {
    char buffer[32];
    unsigned int i = 0;

    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;

    while (i < sizeof(buffer) - 1) {
        if (read(STDIN_FILENO, &buffer[i], 1) != 1) break;
        if (buffer[i] == 'R') break;
        i++;
    }
    buffer[i] = '\0';

    if (buffer[0] != '\x1b' || buffer[1] != '[') return -1;
    if (sscanf(&buffer[2], "%d;%d", rows, cols) != 2) return -1;

    return 0;
}

void Term::initEditor() {
    config_.cursor_x = 0;
    config_.cursor_y = 0;
    config_.r_x = 0;
    config_.row_offset = 0;
    config_.col_offset = 0;
    config_.numrows = 0;
    config_.row = NULL;
    config_.filename = NULL;
    config_.statusMsg[0] = '\0';
    config_.statusMsg_time = 0;

    if (getWindowSize(&config_.screen_rows, &config_.screen_cols) == -1) die("getWindowSize");
    config_.screen_rows -= 2;
}

void Term::editorMoveCursor(int key) {
    trow_ *row = (config_.cursor_y >= config_.numrows) ? NULL : &config_.row[config_.cursor_y];

    switch (key) {
    case ARROW_LEFT:
        if (config_.cursor_x > 0) config_.cursor_x--;
        else if (config_.cursor_y > 0) {
            config_.cursor_y--;
            config_.cursor_x = config_.row[config_.cursor_y].size;
        }
        break;
    case ARROW_RIGHT:
        if (row && config_.cursor_x < row->size) {
            config_.cursor_x++;
        } else if (row && config_.cursor_x == row->size) {
            config_.cursor_y++;
            config_.cursor_x = 0;
        }
        break;
    case ARROW_UP:
        if (config_.cursor_y > 0) config_.cursor_y--;
        break;
    case ARROW_DOWN:
        if (config_.cursor_y < config_.numrows) config_.cursor_y++;
        break;
    }

    row = (config_.cursor_y >= config_.numrows) ? NULL : &config_.row[config_.cursor_y];
    int rowLen = row ? row->size : 0;
    if (config_.cursor_x > rowLen) config_.cursor_x = rowLen;

    config_.r_x = row ? editorRowCxToRx(row, config_.cursor_x) : 0;
}


void Term::editorAppendRow(char *s, size_t len) {
    config_.row = (trow_*)realloc(config_.row, sizeof(trow_) * (config_.numrows + 1));

    int at = config_.numrows;
    config_.row[at].size = len;
    config_.row[at].chars = (char*)malloc(len + 1);
    memcpy(config_.row[at].chars, s, len);
    config_.row[at].chars[len] = '\0';

    config_.row[at].r_size = 0;
    config_.row[at].render = NULL;
    editorUpdateRow(&config_.row[at]);

    config_.numrows++;
}

void Term::editorUpdateRow(trow_ *row) {
    int tabs = 0;
    int j;
    for (j = 0; j < row->size; j++) if (row->chars[j] == '\t') tabs++;

    free(row->render);
    row->render = (char*)malloc(row->size + (tabs * (TAB_STOP - 1)) + 1);

    int idx = 0;
    for (int j = 0; j < row->size; j++) { 
        if (row->chars[j] == '\t') {
            row->render[idx++] = ' ';
            while (idx % TAB_STOP != 0) row->render[idx++] = ' ';
        } else {
            row->render[idx++] = row->chars[j];
        }
    }
    row->render[idx] = '\0';
    row->r_size = idx;
}

void Term::editorOpen(char* filename) {
    free(config_.filename);
    config_.filename = strdup(filename);

    FILE *file = fopen(filename, "r");
    if (!file) die("fopen");

    char *line = NULL;
    size_t lineCap = 0;
    ssize_t lineLen;
    while ((lineLen = getline(&line, &lineCap, file)) != -1) {
        while (lineLen > 0 && (line[lineLen - 1] == '\n' ||
                               line[lineLen - 1] == '\r')) {
            lineLen--;
            editorAppendRow(line, lineLen);
        }
    }
    free(line);
    fclose(file);
}

int Term::editorRowCxToRx(trow_ *row, int cx) {
    int rx = 0;
    int j;
    for (j = 0; j < cx; j++) {
        if (row->chars[j] == '\t') rx += (TAB_STOP - 1) - (rx % TAB_STOP);
        rx++;
    }
    return rx;
}

void Term::editorDrawStatusBar(std::string &ab) {
    ab.append("\x1b[7m", 4);
    char status[80], rstatus[80];
    int len = snprintf(status, sizeof(status), "%.80s - %d lines", 
        config_.filename ? config_.filename : "[No Name]", config_.numrows);
    int rlen = snprintf(rstatus, sizeof(rstatus), "%d/%d",
        config_.cursor_y + 1, config_.numrows);
    if (len > config_.screen_cols) len = config_.screen_cols;
    ab.append(status, len);
    while (len < config_.screen_cols) {
        if (config_.screen_cols - len == rlen) {
            ab.append(rstatus, rlen);
            break;
        } else {
            ab.append(" ", 1);
            len++;
        }
    }
    ab.append("\x1b[m", 3);
    ab.append("\r\n", 2);
}

void Term::editorSetStatusMessage(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(config_.statusMsg, sizeof(config_.statusMsg), fmt, ap);
    va_end(ap);
    config_.statusMsg_time = time(NULL);
}

void Term::editorDrawMessageBar(std::string &ab) {
    ab.append("\x1b[K]", 3);
    int msgLen = strlen(config_.statusMsg);
    if (msgLen > config_.screen_cols) msgLen = config_.screen_cols;
    if (msgLen && time(NULL) - config_.statusMsg_time < 10) ab.append(config_.statusMsg, msgLen);
}