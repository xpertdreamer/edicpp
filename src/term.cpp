/*** defines ***/
#define _DEFAULT_SOURCE
#define _BSD_SOURCE

#define CTRL_KEY(key) ((key) & 0x1f)
#define TAB_STOP 8
#define QUIT_TIMES 3

/*** includes ***/
#include "include/term.hpp"
#include "version.hpp"

/*** usings ***/
using namespace std;

/*** init ***/
Term::Term() : CFG {}{
    enableRawMode();
}

Term::~Term() {
    disableRawMode();
}

/*** methods ***/
void Term::disableRawMode() {
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &CFG.orig_term) == -1) {
        die("tcsetattr in disableRawMode");
    }

    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
}

void Term::enableRawMode() {
    if (tcgetattr(STDIN_FILENO, &CFG.orig_term) == -1) {
        die("tcgetattr");
    }
    
    struct termios raw = CFG.orig_term;
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
            } else if (seq[1] >= '0' && seq [1] <= '9') {
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
    static int quit_times = QUIT_TIMES;

    int c = editorReadKey();

    switch(c) {
        case CTRL_KEY('s'):
            editorSave();
            break;

        case '\r':
            editorInsertNewLine();
            break;

        case CTRL_KEY('q'):
            if (CFG.dirty && quit_times > 0){
                editorSetStatusMessage("Hey! This file is modified. "
                    "Press CTRL-Q %d more times to quit", quit_times);
                quit_times--;
                return true;
            } else return false;

        case CTRL_ARROW_UP:
            // fall through
        case CTRL_ARROW_DOWN:
        {
            if (c == CTRL_ARROW_UP) CFG.cursor_y = CFG.row_offset;
            else if (c == CTRL_ARROW_DOWN) {
                CFG.cursor_y = CFG.row_offset + CFG.screen_rows - 1;
                if (CFG.cursor_y > CFG.numrows) CFG.cursor_y = CFG.numrows;
            }

            int times = CFG.screen_rows;
            while (times--) {
                editorMoveCursor(c == CTRL_ARROW_UP ? ARROW_UP : ARROW_DOWN);
            }
            break;
        }

        case CTRL_KEY('f'):
            editorFind();
            break;

        case CTRL_ARROW_RIGHT:
            if (CFG.cursor_y < CFG.numrows) CFG.cursor_x = CFG.row[CFG.cursor_y].size;
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

        case DEL_KEY:
        case BACKSPACE:
        case CTRL_KEY('h'):
            if (c == DEL_KEY) editorMoveCursor(ARROW_RIGHT);
            editorDelChar();

        case CTRL_KEY('l'):
        case '\x1b':
            break;

        default:
            editorInsertChar(c);
            break;
    }

    quit_times = QUIT_TIMES;
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
    snprintf(buffer, sizeof(buffer), "\x1b[%d;%dH", (CFG.cursor_y - CFG.row_offset) + 1, (CFG.r_x - CFG.col_offset) + 1);
    ab += buffer;

    ab += "\x1b[?25h";

    write(STDOUT_FILENO, ab.c_str(), ab.length());
}

void Term::editorDrawRows(string &ab) {
    for (int y = 0; y < CFG.screen_rows; y++) {
        int fileRow = y + CFG.row_offset;
        if (fileRow >= CFG.numrows) {
            if (CFG.numrows == 0 && y == CFG.screen_rows / 2) {
                char welcome_msg[80];
                int welcomelen = snprintf(welcome_msg, sizeof(welcome_msg),
                "%s -- version %s", 
                PROJECT_NAME, 
                PROJECT_VERSION);

            if (welcomelen > CFG.screen_cols) welcomelen = CFG.screen_cols;
            int padding = (CFG.screen_cols - welcomelen) / 2;
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
            int len = CFG.row[fileRow].r_size - CFG.col_offset;
            if (len < 0) len = 0;
            if (len > CFG.screen_cols) len = CFG.screen_cols;
            char *c = &CFG.row[fileRow].render[CFG.col_offset];
            unsigned char *hl = &CFG.row[fileRow].hl[CFG.col_offset];
            int current_color = -1;
            for (int j = 0; j < len; j++) {
                if (hl[j] == HL_NORMAL) {
                    if (current_color != -1) {
                        ab.append("\x1b[39m", 5);
                        current_color = -1;
                    }
                    ab.append(&c[j], 1);
                } else {
                    int color = editorSyntaxToColor(hl[j]);
                    if (color != current_color) {
                        current_color = color;
                        char seq[16];
                        int n = snprintf(seq, sizeof(seq), "\x1b[%dm", color);
                        ab.append(seq, n);
                    }
                    ab.append(&c[j], 1);
                }
            }
            ab.append("\x1b[39m", 5);
        }
            ab += "\x1b[K";
            ab += "\r\n";
    }
}

void Term::editorScroll() {
    CFG.r_x = 0;
    if (CFG.cursor_y < CFG.numrows) CFG.r_x = editorRowCxToRx(&CFG.row[CFG.cursor_y], CFG.cursor_x);

    if (CFG.cursor_y < CFG.row_offset) CFG.row_offset = CFG.cursor_y;
    if (CFG.cursor_y >= CFG.row_offset + CFG.screen_rows) CFG.row_offset = CFG.cursor_y - CFG.screen_rows + 1;
    if (CFG.r_x < CFG.col_offset) CFG.col_offset = CFG.r_x;
    if (CFG.r_x >= CFG.col_offset + CFG.screen_cols) CFG.col_offset = CFG.r_x - CFG.screen_cols + 1;
}

int Term::getWindowSize(int *rows, int *cols) {
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
        return getCursorPosition(rows, cols);
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
    CFG.cursor_x = 0;
    CFG.cursor_y = 0;
    CFG.r_x = 0;
    CFG.row_offset = 0;
    CFG.col_offset = 0;
    CFG.numrows = 0;
    CFG.row = NULL;
    CFG.filename = NULL;
    CFG.dirty = 0;
    CFG.statusMsg[0] = '\0';
    CFG.statusMsg_time = 0;

    if (getWindowSize(&CFG.screen_rows, &CFG.screen_cols) == -1) die("getWindowSize");
    CFG.screen_rows -= 2;
}

void Term::editorMoveCursor(int key) {
    trow_ *row = (CFG.cursor_y >= CFG.numrows) ? NULL : &CFG.row[CFG.cursor_y];

    switch (key) {
    case CTRL_ARROW_LEFT:
        CFG.cursor_x = 0;
        break;    
    case ARROW_LEFT:
        if (CFG.cursor_x > 0) CFG.cursor_x--;
        else if (CFG.cursor_y > 0) {
            CFG.cursor_y--;
            CFG.cursor_x = CFG.row[CFG.cursor_y].size;
        }
        break;
    case ARROW_RIGHT:
        if (row && CFG.cursor_x < row->size) {
            CFG.cursor_x++;
        } else if (row && CFG.cursor_x == row->size) {
            CFG.cursor_y++;
            CFG.cursor_x = 0;
        }
        break;
    case ARROW_UP:
        if (CFG.cursor_y > 0) CFG.cursor_y--;
        break;
    case ARROW_DOWN:
        if (CFG.cursor_y < CFG.numrows) CFG.cursor_y++;
        break;
    }

    row = (CFG.cursor_y >= CFG.numrows) ? NULL : &CFG.row[CFG.cursor_y];
    int rowLen = row ? row->size : 0;
    if (CFG.cursor_x > rowLen) CFG.cursor_x = rowLen;

    CFG.r_x = row ? editorRowCxToRx(row, CFG.cursor_x) : 0;
}


void Term::editorInsertRow(int at, char *s, size_t len) {
    if (at < 0 || at > CFG.numrows) return;

    CFG.row = (trow_*)realloc(CFG.row, sizeof(trow_) * (CFG.numrows + 1));
    memmove(&CFG.row[at + 1], &CFG.row[at], sizeof(trow_) * (CFG.numrows - at));

    CFG.row[at].size = len;
    CFG.row[at].chars = (char*)malloc(len + 1);
    memcpy(CFG.row[at].chars, s, len);
    CFG.row[at].chars[len] = '\0';

    CFG.row[at].r_size = 0;
    CFG.row[at].render = NULL;
    CFG.row[at].hl = NULL;
    editorUpdateRow(&CFG.row[at]);

    CFG.numrows++;
    CFG.dirty ++;
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

    editorUpdateSyntax(row);
}

void Term::editorOpen(char* filename) {
    free(CFG.filename);
    CFG.filename = strdup(filename);

    FILE *file = fopen(filename, "r");
    if (!file) die("fopen");

    char *line = NULL;
    size_t lineCap = 0;
    ssize_t lineLen;
    while ((lineLen = getline(&line, &lineCap, file)) != -1) {
        while (lineLen > 0 && (line[lineLen - 1] == '\n' ||
                               line[lineLen - 1] == '\r')) {
            lineLen--;
        }
        editorInsertRow(CFG.numrows, line, lineLen);
    }
    free(line);
    fclose(file);
    CFG.dirty = 0;
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
    int len = snprintf(status, sizeof(status), "%.80s - %d lines %s", 
        CFG.filename ? CFG.filename : "[No Name]", CFG.numrows,
        CFG.dirty ? "(modified)" : "");
    int rlen = snprintf(rstatus, sizeof(rstatus), "%d/%d",
        CFG.cursor_y + 1, CFG.numrows);
    if (len > CFG.screen_cols) len = CFG.screen_cols;
    ab.append(status, len);
    while (len < CFG.screen_cols) {
        if (CFG.screen_cols - len == rlen) {
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
    vsnprintf(CFG.statusMsg, sizeof(CFG.statusMsg), fmt, ap);
    va_end(ap);
    CFG.statusMsg_time = time(NULL);
}

void Term::editorDrawMessageBar(std::string &ab) {
    ab.append("\x1b[K", 3);
    int msgLen = strlen(CFG.statusMsg);
    if (msgLen > CFG.screen_cols) msgLen = CFG.screen_cols;
    if (msgLen && time(NULL) - CFG.statusMsg_time < 7) ab.append(CFG.statusMsg, msgLen);
}

void Term::editorRowInsertChar(trow_ *row, int at, int c) {
    if (at < 0 || at > row->size) at = row->size;
    row->chars = (char*)realloc(row->chars, row->size + 2);
    memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
    row->size++;
    row->chars[at] = c;
    editorUpdateRow(row);
    CFG.dirty++;
}

void Term::editorInsertChar(int c) {
    if (CFG.cursor_y == CFG.numrows) editorInsertRow(CFG.numrows, (char*)"", 0);
    editorRowInsertChar(&CFG.row[CFG.cursor_y], CFG.cursor_x, c);
    CFG.cursor_x++;
}

char *Term::editorRowToString(int *buflen) {
    int total_len = 0;
    int j;
    for (j = 0; j < CFG.numrows; j++) total_len += CFG.row[j].size + 1;
    *buflen = total_len;

    char *buf = (char*)malloc(total_len);
    char *p = buf;
    for (j = 0; j < CFG.numrows; j++) {
        memcpy(p, CFG.row[j].chars, CFG.row[j].size);
        p += CFG.row[j].size;
        *p = '\n';
        p++;
    }

    return buf;
}

void Term::editorSave() {
    if (CFG.filename == NULL){
        CFG.filename = editorPrompt((char*)"Save as: %s", NULL);
        if (CFG.filename == NULL) {
            editorSetStatusMessage("Save aborted");
            return;
        }
    }

    int len;
    char *buf = editorRowToString(&len);

    int fd = open(CFG.filename, O_RDWR | O_CREAT, 0644);
    if (fd != -1) {
        if (ftruncate(fd, len) != -1) {
            if (write(fd, buf, len) == len) {
                close(fd);
                free(buf);
                editorSetStatusMessage("%d bytes written to disk", len);
                return;
            }
        }
        close(fd);
    }
    free(buf);
    editorSetStatusMessage("Oops. I/O error: %s", strerror(errno));
}

void Term::editorRowDeleteChar(trow_ *row, int at) {
    if (at < 0 || at >= row->size) return;
    memmove(&row->chars[at], &row->chars[at+1], row->size - at);
    row->size--;
    editorUpdateRow(row);
    CFG.dirty++;
}

void Term::editorDelChar() {
    if (CFG.cursor_y == CFG.numrows) return;
    if (CFG.cursor_x == 0 && CFG.cursor_y == 0) return;

    trow_ *row = &CFG.row[CFG.cursor_y];
    if (CFG.cursor_x > 0) {
        editorRowDeleteChar(row, CFG.cursor_x - 1);
        CFG.cursor_x--;
    } else {
        CFG.cursor_x = CFG.row[CFG.cursor_y - 1].size;
        editorRowAppendString(&CFG.row[CFG.cursor_y - 1], row->chars, row->size);
        editorDelRow(CFG.cursor_y);
        CFG.cursor_y--;
    }
}

void Term::editorFreeRow(trow_ *row) {
    free(row->render);
    free(row->chars);
    free(row->hl);
}

void Term::editorDelRow(int at) {
    if (at < 0 || at >= CFG.numrows) return;
    editorFreeRow(&CFG.row[at]);
    memmove(&CFG.row[at], &CFG.row[at + 1], sizeof(trow_) * (CFG.numrows - at - 1));
    CFG.numrows--;
    CFG.dirty++;
}

void Term::editorRowAppendString(trow_ *row, char *s, size_t len) {
    row->chars = (char *)realloc(row->chars, row->size + len + 1);
    memcpy(&row->chars[row->size], s, len);
    row->size += len;
    row->chars[row->size] = '\0';
    editorUpdateRow(row);
    CFG.dirty++;
}

void Term::editorInsertNewLine() {
    if (CFG.cursor_x == 0) editorInsertRow(CFG.cursor_y, (char*)"", 0);
    else {
        trow_ *row = &CFG.row[CFG.cursor_y];
        editorInsertRow(CFG.cursor_y + 1, &row->chars[CFG.cursor_x], row->size - CFG.cursor_x);
        row = &CFG.row[CFG.cursor_y];
        row->size = CFG.cursor_x;
        row->chars[row->size] = '\0';
        editorUpdateRow(row);
    }
    CFG.cursor_y++;
    CFG.cursor_x = 0;
}

char *Term::editorPrompt(char *prompt, std::function<void(char*, int)> callback) {
    size_t bufsize = 128;
    char *buf = (char *)malloc(bufsize);
    size_t buflen = 0;
    buf[0] = '\0';

    while (true) {
        editorSetStatusMessage(prompt, buf);
        editorRefreshScreen();

        int c = editorReadKey();
        if (c == DEL_KEY || c == CTRL_KEY('h') || c == BACKSPACE) {
            if (buflen != 0 ) buf[--buflen] = '\0';
        } else if (c == '\x1b') {
            editorSetStatusMessage("");
            if (callback) callback(buf, c);
            free(buf);
            return NULL;
        } else if (c == '\r') {
            if (buflen != 0) {
                editorSetStatusMessage("");
                if (callback) callback(buf, c);
                return buf;
            }
        } else if (!iscntrl(c) && c < 128) {
            if (buflen == bufsize - 1) {
                bufsize *= 2;
                buf = (char *)realloc(buf, bufsize);
            }
            buf[buflen++] = c;
            buf[buflen] = '\0';
        }

        if (callback) callback(buf, c);
    }
}

void Term::editorFind() {
    int saved_cx = CFG.cursor_x;
    int saved_cy = CFG.cursor_y;
    int saved_coloff = CFG.col_offset;
    int saved_rowoff = CFG.row_offset;

    char *query = editorPrompt(
        (char*)"Search: %s (HELP: ESC/Arrows/Enter)",
        [this](char *query, int key){ this->editorFindCallback(query, key); }
    );

    if (query) free(query);
    else {
        CFG.cursor_x = saved_cx;
        CFG.cursor_y = saved_cy;
        CFG.col_offset = saved_coloff;
        CFG.row_offset = saved_rowoff;
    }
}

void Term::editorFindCallback(char *query, int key) {
    static int last_match = -1;
    static int direction = 1;

    static int saved_hl_line;
    static char *saved_hl = NULL;

    if (saved_hl) {
        memcpy(CFG.row[saved_hl_line].hl, saved_hl, CFG.row[saved_hl_line].r_size);
        free(saved_hl);
        saved_hl = NULL;
    }

    if (key == '\r' || key == '\x1b') {
        last_match = -1;
        direction = 1;
        return;
    } 
    else if (key == ARROW_RIGHT || key == ARROW_DOWN) direction = 1;
    else if (key == ARROW_LEFT || key == ARROW_UP) direction = -1;
    else {
        last_match = -1;
        direction = 1;
    }

    if (last_match == -1) direction = 1;
    int curr = last_match;
    for (int i = 0; i < CFG.numrows; i++) {
        curr += direction;
        if (curr == -1) curr = CFG.numrows - 1;
        else if (curr == CFG.numrows) curr = 0;

        trow_ *row = &CFG.row[curr];
        char *match = strstr(row->render, query);
        if (match) {
            last_match = curr;
            CFG.cursor_y = curr;
            CFG.cursor_x = editorRowRxToCx(row, match - row->render);
            CFG.row_offset = CFG.numrows;

            saved_hl_line = curr;
            saved_hl = (char *)malloc(row->r_size);
            memcpy(saved_hl, row->hl, row->r_size);
            memset(&row->hl[match - row->render], HL_MATCH, strlen(query));
            break;
        }
    }
}

int Term::editorRowRxToCx(trow_ *row, int rx) {
    int cur_rx = 0;
    int cx;
    for (cx = 0; cx < row->size; cx++) {
        if (row->chars[cx] == '\t') cur_rx += (TAB_STOP - 1) - (cur_rx % TAB_STOP);
        cur_rx++;

        if (cur_rx > rx) return cx;
    }
    return cx;
}

void Term::editorUpdateSyntax(trow_ *row) {
    row->hl = (unsigned char *)realloc(row->hl, row->r_size);
    memset(row->hl, HL_NORMAL, row->r_size);

    for(int i = 0; i < row->r_size; i++) {
        if (isdigit(row->render[i])) row->hl[i] = HL_NUMBER;
    }
}

int Term::editorSyntaxToColor(int hl) {
    switch (hl) {
        case HL_NUMBER: return 34;
        case HL_MATCH: return 33;
        default: return 37;
    }
}