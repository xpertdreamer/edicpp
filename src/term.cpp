/*** defines ***/
#define _DEFAULT_SOURCE
#define _BSD_SOURCE

#define HL_HIGHLIGHT_NUMBERS (1<<0)
#define HL_HIGHLIGHT_STRINGS (1<<1)
#define CTRL_KEY(key) ((key) & 0x1f)

/*** includes ***/
#include "include/term.hpp"
#include "include/config.hpp"
#include "version.hpp"

/*** usings ***/
using namespace std;

/*** init ***/
Config cfg;

Term::Term() : _C {} {
    const char* home = std::getenv("HOME");
    if (!home) die("Не удалось получить HOME");

    namespace fs = std::filesystem;
    fs::path configDir;

    #ifdef __APPLE__
    configDir = fs::path(home) / "Library" / "Application Support" / "edi";
    #else
    configDir = fs::path(home) / ".config" / "edi";
    #endif

    std::error_code ec;
    if (!fs::exists(configDir) && !fs::create_directories(configDir, ec)) {
        std::cerr << "Не удалось создать папку конфигурации: " << ec.message() << std::endl;
        die("create_directories");
    }

    fs::path configPath = configDir / "config.conf";

    if (!fs::exists(configPath)) {
        std::ofstream ofs(configPath);
        if (!ofs) {
            std::cerr << "Не удалось создать конфигурационный файл: " 
                      << configPath << std::endl;
            die("create_config_file");
        }
    }

    if (cfg.loadConfig(configPath.string()) == 1) die("loadConfig");

    enableRawMode();
}

Term::~Term() {
    disableRawMode();
}

struct editorSyntax {
    const char *filetype;
    const char **filematch;
    const char **keywords;
    const char *single_line_comment_start;
    const char *multiline_comment_start;
    const char *multiline_comment_end;
    int flags;
};

const char *C_HL_extensions[] = { ".c", ".h", ".cpp", ".hpp", nullptr };
const char *C_HL_keywords[] = {
    "switch", "if", "while", "for", "break", "continue", "return", "else",
    "struct", "union", "typedef", "static", "enum", "class", "case",
    "int|", "long|", "double|", "float|", "char|", "unsigned|", "signed|",
    "void|", nullptr
};

editorSyntax HLDB[] = {
    {
        "c",
        C_HL_extensions,
        C_HL_keywords,
        "//", "/*", "*/",
        HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS
    }
};

#define HLDB_ENTRIES (sizeof(HLDB) / sizeof(HLDB[0]))

/*** methods ***/
void Term::disableRawMode() {
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &_C.orig_term) == -1) {
        die("tcsetattr in disableRawMode");
    }

    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
}

void Term::enableRawMode() {
    if (tcgetattr(STDIN_FILENO, &_C.orig_term) == -1) {
        die("tcgetattr");
    }
    
    struct termios raw = _C.orig_term;
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
    static int quit_times = cfg.config.quit_times;

    int c = editorReadKey();

    switch(c) {
        case CTRL_KEY('s'):
            editorSave();
            break;

        case '\r':
            editorInsertNewLine();
            break;

        case CTRL_KEY('q'):
            if (_C.dirty && quit_times > 0){
                editorSetStatusMessage("Hey! This file is modified. "
                    "Press CTRL-Q %d more times to quit", quit_times);
                quit_times--;
                return true;
            } else return false;

        case CTRL_ARROW_UP:
            // fall through
        case CTRL_ARROW_DOWN:
        {
            if (c == CTRL_ARROW_UP) _C.cursor_y = _C.row_offset;
            else if (c == CTRL_ARROW_DOWN) {
                _C.cursor_y = _C.row_offset + _C.screen_rows - 1;
                if (_C.cursor_y > _C.numrows) _C.cursor_y = _C.numrows;
            }

            int times = _C.screen_rows;
            while (times--) {
                editorMoveCursor(c == CTRL_ARROW_UP ? ARROW_UP : ARROW_DOWN);
            }
            break;
        }

        case CTRL_KEY('f'):
            editorFind();
            break;

        case CTRL_ARROW_RIGHT:
            if (_C.cursor_y < _C.numrows) _C.cursor_x = _C.row[_C.cursor_y].size;
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

    quit_times = cfg.config.quit_times;
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
    snprintf(buffer, sizeof(buffer), "\x1b[%d;%dH", (_C.cursor_y - _C.row_offset) + 1, (_C.r_x - _C.col_offset) + 1);
    ab += buffer;

    ab += "\x1b[?25h";

    write(STDOUT_FILENO, ab.c_str(), ab.length());
}

void Term::editorDrawRows(string &ab) {
    for (int y = 0; y < _C.screen_rows; y++) {
        int fileRow = y + _C.row_offset;
        if (fileRow >= _C.numrows) {
            if (_C.numrows == 0 && y == _C.screen_rows / 2) {
                char welcome_msg[80];
                int welcomelen = snprintf(welcome_msg, sizeof(welcome_msg),
                "%s -- version %s", 
                PROJECT_NAME, 
                PROJECT_VERSION);

            if (welcomelen > _C.screen_cols) welcomelen = _C.screen_cols;
            int padding = (_C.screen_cols - welcomelen) / 2;
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
            int len = _C.row[fileRow].r_size - _C.col_offset;
            if (len < 0) len = 0;
            if (len > _C.screen_cols) len = _C.screen_cols;
            char *c = &_C.row[fileRow].render[_C.col_offset];
            unsigned char *hl = &_C.row[fileRow].hl[_C.col_offset];
            int current_color = -1;
            for (int j = 0; j < len; j++) {
                if (iscntrl(c[j])) {
                    char sym = (c[j] <= 26) ? '@' + c[j] : '?';
                    ab.append("\x1b[7m", 4);
                    ab.append(&sym, 1);
                    ab.append("\x1b[m", 3);
                    if (current_color != -1) {
                        char seq[16];
                        int n = snprintf(seq, sizeof(seq), "\x1b[%dm", current_color);
                        ab.append(seq, n);
                    }
                } else if (hl[j] == HL_NORMAL) {
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
                        int n = snprintf(seq, sizeof(seq), "\x1b[1;%dm", color);
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
    _C.r_x = 0;
    if (_C.cursor_y < _C.numrows) _C.r_x = editorRowCxToRx(&_C.row[_C.cursor_y], _C.cursor_x);

    if (_C.cursor_y < _C.row_offset) _C.row_offset = _C.cursor_y;
    if (_C.cursor_y >= _C.row_offset + _C.screen_rows) _C.row_offset = _C.cursor_y - _C.screen_rows + 1;
    if (_C.r_x < _C.col_offset) _C.col_offset = _C.r_x;
    if (_C.r_x >= _C.col_offset + _C.screen_cols) _C.col_offset = _C.r_x - _C.screen_cols + 1;
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
    _C.cursor_x = 0;
    _C.cursor_y = 0;
    _C.r_x = 0;
    _C.row_offset = 0;
    _C.col_offset = 0;
    _C.numrows = 0;
    _C.row = NULL;
    _C.filename = NULL;
    _C.dirty = 0;
    _C.statusMsg[0] = '\0';
    _C.statusMsg_time = 0;
    _C.syntax = NULL;

    if (getWindowSize(&_C.screen_rows, &_C.screen_cols) == -1) die("getWindowSize");
    _C.screen_rows -= 2;
}

void Term::editorMoveCursor(int key) {
    trow_ *row = (_C.cursor_y >= _C.numrows) ? NULL : &_C.row[_C.cursor_y];

    switch (key) {
    case CTRL_ARROW_LEFT:
        _C.cursor_x = 0;
        break;    
    case ARROW_LEFT:
        if (_C.cursor_x > 0) _C.cursor_x--;
        else if (_C.cursor_y > 0) {
            _C.cursor_y--;
            _C.cursor_x = _C.row[_C.cursor_y].size;
        }
        break;
    case ARROW_RIGHT:
        if (row && _C.cursor_x < row->size) {
            _C.cursor_x++;
        } else if (row && _C.cursor_x == row->size) {
            _C.cursor_y++;
            _C.cursor_x = 0;
        }
        break;
    case ARROW_UP:
        if (_C.cursor_y > 0) _C.cursor_y--;
        break;
    case ARROW_DOWN:
        if (_C.cursor_y < _C.numrows) _C.cursor_y++;
        break;
    }

    row = (_C.cursor_y >= _C.numrows) ? NULL : &_C.row[_C.cursor_y];
    int rowLen = row ? row->size : 0;
    if (_C.cursor_x > rowLen) _C.cursor_x = rowLen;

    _C.r_x = row ? editorRowCxToRx(row, _C.cursor_x) : 0;
}


void Term::editorInsertRow(int at, char *s, size_t len) {
    if (at < 0 || at > _C.numrows) return;

    _C.row = (trow_*)realloc(_C.row, sizeof(trow_) * (_C.numrows + 1));
    memmove(&_C.row[at + 1], &_C.row[at], sizeof(trow_) * (_C.numrows - at));
    for (int j = at + 1; j <= _C.numrows; j++) _C.row[j].idx++;

    _C.row[at].idx = at;

    _C.row[at].size = len;
    _C.row[at].chars = (char*)malloc(len + 1);
    memcpy(_C.row[at].chars, s, len);
    _C.row[at].chars[len] = '\0';

    _C.row[at].r_size = 0;
    _C.row[at].render = NULL;
    _C.row[at].hl = NULL;
    _C.row[at].hl_open_comment = 0;
    editorUpdateRow(&_C.row[at]);

    _C.numrows++;
    _C.dirty ++;
}

void Term::editorUpdateRow(trow_ *row) {
    int tabs = 0;
    int j;
    for (j = 0; j < row->size; j++) if (row->chars[j] == '\t') tabs++;

    free(row->render);
    row->render = (char*)malloc(row->size + (tabs * (cfg.config.tab_stop - 1)) + 1);

    int idx = 0;
    for (int j = 0; j < row->size; j++) { 
        if (row->chars[j] == '\t') {
            row->render[idx++] = ' ';
            while (idx % cfg.config.tab_stop != 0) row->render[idx++] = ' ';
        } else {
            row->render[idx++] = row->chars[j];
        }
    }
    row->render[idx] = '\0';
    row->r_size = idx;

    editorUpdateSyntax(row);
}

void Term::editorOpen(char* filename) {
    free(_C.filename);
    _C.filename = strdup(filename);

    editorSelectSyntaxHighlight();

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
        editorInsertRow(_C.numrows, line, lineLen);
    }
    free(line);
    fclose(file);
    _C.dirty = 0;
}

int Term::editorRowCxToRx(trow_ *row, int cx) {
    int rx = 0;
    int j;
    for (j = 0; j < cx; j++) {
        if (row->chars[j] == '\t') rx += (cfg.config.tab_stop - 1) - (rx % cfg.config.tab_stop);
        rx++;
    }
    return rx;
}

void Term::editorDrawStatusBar(std::string &ab) {
    ab.append("\x1b[7m", 4);
    char status[80], rstatus[80];
    int len = snprintf(status, sizeof(status), "%.80s - %d lines %s", 
        _C.filename ? _C.filename : "[No Name]", _C.numrows,
        _C.dirty ? "(modified)" : "");
    int rlen = snprintf(rstatus, sizeof(rstatus), "%s | %d/%d",
        _C.syntax ? _C.syntax->filetype : "no ft", _C.cursor_y + 1, _C.numrows);
    if (len > _C.screen_cols) len = _C.screen_cols;
    ab.append(status, len);
    while (len < _C.screen_cols) {
        if (_C.screen_cols - len == rlen) {
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
    vsnprintf(_C.statusMsg, sizeof(_C.statusMsg), fmt, ap);
    va_end(ap);
    _C.statusMsg_time = time(NULL);
}

void Term::editorDrawMessageBar(std::string &ab) {
    ab.append("\x1b[K", 3);
    int msgLen = strlen(_C.statusMsg);
    if (msgLen > _C.screen_cols) msgLen = _C.screen_cols;
    if (msgLen && time(NULL) - _C.statusMsg_time < 7) ab.append(_C.statusMsg, msgLen);
}

void Term::editorRowInsertChar(trow_ *row, int at, int c) {
    if (at < 0 || at > row->size) at = row->size;
    row->chars = (char*)realloc(row->chars, row->size + 2);
    memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
    row->size++;
    row->chars[at] = c;
    editorUpdateRow(row);
    _C.dirty++;
}

void Term::editorInsertChar(int c) {
    if (_C.cursor_y == _C.numrows) editorInsertRow(_C.numrows, (char*)"", 0);
    editorRowInsertChar(&_C.row[_C.cursor_y], _C.cursor_x, c);
    _C.cursor_x++;
}

char *Term::editorRowToString(int *buflen) {
    int total_len = 0;
    int j;
    for (j = 0; j < _C.numrows; j++) total_len += _C.row[j].size + 1;
    *buflen = total_len;

    char *buf = (char*)malloc(total_len);
    char *p = buf;
    for (j = 0; j < _C.numrows; j++) {
        memcpy(p, _C.row[j].chars, _C.row[j].size);
        p += _C.row[j].size;
        *p = '\n';
        p++;
    }

    return buf;
}

void Term::editorSave() {
    if (_C.filename == NULL){
        _C.filename = editorPrompt((char*)"Save as: %s", NULL);
        if (_C.filename == NULL) {
            editorSetStatusMessage("Save aborted");
            return;
        }
        editorSelectSyntaxHighlight();
    }

    int len;
    char *buf = editorRowToString(&len);

    int fd = open(_C.filename, O_RDWR | O_CREAT, 0644);
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
    _C.dirty++;
}

void Term::editorDelChar() {
    if (_C.cursor_y == _C.numrows) return;
    if (_C.cursor_x == 0 && _C.cursor_y == 0) return;

    trow_ *row = &_C.row[_C.cursor_y];
    if (_C.cursor_x > 0) {
        editorRowDeleteChar(row, _C.cursor_x - 1);
        _C.cursor_x--;
    } else {
        _C.cursor_x = _C.row[_C.cursor_y - 1].size;
        editorRowAppendString(&_C.row[_C.cursor_y - 1], row->chars, row->size);
        editorDelRow(_C.cursor_y);
        _C.cursor_y--;
    }
}

void Term::editorFreeRow(trow_ *row) {
    free(row->render);
    free(row->chars);
    free(row->hl);
}

void Term::editorDelRow(int at) {
    if (at < 0 || at >= _C.numrows) return;
    editorFreeRow(&_C.row[at]);
    memmove(&_C.row[at], &_C.row[at + 1], sizeof(trow_) * (_C.numrows - at - 1));
    for (int j = at; j < _C.numrows - 1; j++) _C.row[j].idx--;
    _C.numrows--;
    _C.dirty++;
}

void Term::editorRowAppendString(trow_ *row, char *s, size_t len) {
    row->chars = (char *)realloc(row->chars, row->size + len + 1);
    memcpy(&row->chars[row->size], s, len);
    row->size += len;
    row->chars[row->size] = '\0';
    editorUpdateRow(row);
    _C.dirty++;
}

void Term::editorInsertNewLine() {
    if (_C.cursor_x == 0) editorInsertRow(_C.cursor_y, (char*)"", 0);
    else {
        trow_ *row = &_C.row[_C.cursor_y];
        editorInsertRow(_C.cursor_y + 1, &row->chars[_C.cursor_x], row->size - _C.cursor_x);
        row = &_C.row[_C.cursor_y];
        row->size = _C.cursor_x;
        row->chars[row->size] = '\0';
        editorUpdateRow(row);
    }
    _C.cursor_y++;
    _C.cursor_x = 0;
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
    int saved_cx = _C.cursor_x;
    int saved_cy = _C.cursor_y;
    int saved_coloff = _C.col_offset;
    int saved_rowoff = _C.row_offset;

    char *query = editorPrompt(
        (char*)"Search: %s (HELP: ESC/Arrows/Enter)",
        [this](char *query, int key){ this->editorFindCallback(query, key); }
    );

    if (query) free(query);
    else {
        _C.cursor_x = saved_cx;
        _C.cursor_y = saved_cy;
        _C.col_offset = saved_coloff;
        _C.row_offset = saved_rowoff;
    }
}

void Term::editorFindCallback(char *query, int key) {
    static int last_match = -1;
    static int direction = 1;

    static int saved_hl_line;
    static char *saved_hl = NULL;

    if (saved_hl) {
        memcpy(_C.row[saved_hl_line].hl, saved_hl, _C.row[saved_hl_line].r_size);
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
    for (int i = 0; i < _C.numrows; i++) {
        curr += direction;
        if (curr == -1) curr = _C.numrows - 1;
        else if (curr == _C.numrows) curr = 0;

        trow_ *row = &_C.row[curr];
        char *match = strstr(row->render, query);
        if (match) {
            last_match = curr;
            _C.cursor_y = curr;
            _C.cursor_x = editorRowRxToCx(row, match - row->render);
            _C.row_offset = _C.numrows;

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
        if (row->chars[cx] == '\t') cur_rx += (cfg.config.tab_stop - 1) - (cur_rx % cfg.config.tab_stop);
        cur_rx++;

        if (cur_rx > rx) return cx;
    }
    return cx;
}

void Term::editorUpdateSyntax(trow_ *row) {
    row->hl = (unsigned char *)realloc(row->hl, row->r_size);
    memset(row->hl, HL_NORMAL, row->r_size);

    if (_C.syntax == NULL) return;

    const char **keywords = _C.syntax->keywords;

    const char *scs = _C.syntax->single_line_comment_start;
    const char *mcs = _C.syntax->multiline_comment_start;
    const char *mce = _C.syntax->multiline_comment_end;

    int scs_len = scs ? strlen(scs) : 0;
    int mcs_len = mcs ? strlen(mcs) : 0;
    int mce_len = mce ? strlen(mce) : 0;

    bool prev_sep = 1;
    bool in_string = 0;
    bool in_comment = (row->idx > 0 && _C.row[row->idx - 1].hl_open_comment);

    int i = 0;
    while (i < row->r_size) {
        char c = row->render[i];
        unsigned char prev_hl = (i > 0) ? row->hl[i - 1] : (unsigned char)HL_NORMAL;

        if (scs_len && !in_string && !in_comment) {
            if (!strncmp(&row->render[i], scs, scs_len)) {
                memset(&row->hl[i], HL_COMMENT, row->r_size - i);
                break;
            }
        }

        if (mcs_len && mce_len && !in_string) {
            if (in_comment) {
                row->hl[i] = HL_MLCOMMENT;
                if (!strncmp(&row->render[i], mce, mce_len)) {
                    memset(&row->hl[i], HL_MLCOMMENT, mce_len);
                    i += mce_len;
                    in_comment = 0;
                    prev_sep = 1;
                    continue;
                } else {
                    i++;
                    continue;
                }
            } else if (!strncmp(&row->render[i], mcs, mcs_len)) {
                memset(&row->hl[i], HL_MLCOMMENT, mcs_len);
                i += mcs_len;
                in_comment = 1;
                continue;
            }
        }

        if (_C.syntax->flags & HL_HIGHLIGHT_STRINGS) {
            if (in_string) {
                row->hl[i] = HL_STRING;
                if (c == '\\' && i + 1 < row->r_size) {
                    row->hl[i + 1] = HL_STRING;
                    i += 2;
                    continue;
                }
                if (c == in_string) in_string = 0;
                i++;
                prev_sep = 1;
                continue;
            } else if (c == '"' || c == '\'') {
                in_string = c;
                row->hl[i] = HL_STRING;
                i++;
                continue;
            }
        }

        if(_C.syntax->flags & HL_HIGHLIGHT_NUMBERS) {
            if ((isdigit(c) && (prev_sep || prev_hl == HL_NUMBER)) ||
                    (c == '.' && prev_hl == HL_NUMBER)){
                row->hl[i] = HL_NUMBER;
                i++;
                prev_sep = 0;
                continue;
            }
        }

        if (prev_sep) {
            int j;
            for (j = 0; keywords[j]; j++) {
                int klen = strlen(keywords[j]);
                int kw2 = keywords[j][klen - 1] == '|';
                if (kw2) klen--;

                if (!strncmp(&row->render[i], keywords[j], klen) &&
                        is_separator(row->render[i + klen])) {
                    memset(&row->hl[i], kw2 ? HL_KEYWORD2 : HL_KEYWORD1, klen);
                    i += klen;
                    break;
                }
            }
            if (keywords[j] != NULL) {
                prev_sep = 0;
                continue;
            }
        }

        prev_sep = is_separator(c);
        i++;
    }

    int changed = (row->hl_open_comment != in_comment);
    row->hl_open_comment = in_comment;
    if (changed && row->idx + 1 < _C.numrows) editorUpdateSyntax(&_C.row[row->idx + 1]);
}

int Term::editorSyntaxToColor(int hl) {
    switch (hl) {
        case HL_MLCOMMENT:
        case HL_COMMENT: return cfg.config.hl_comment;
        case HL_KEYWORD1: return cfg.config.hl_keyword1;
        case HL_KEYWORD2: return cfg.config.hl_keyword2;
        case HL_NUMBER: return cfg.config.hl_number;
        case HL_MATCH: return cfg.config.hl_match;
        case HL_STRING: return cfg.config.hl_string;
        default: return 37;
    }
}

int Term::is_separator(int c) {
    return isspace(c) || c == '\0' || strchr(",.()+-/*=~%<>[];", c) != NULL;
}

void Term::editorSelectSyntaxHighlight() {
    _C.syntax = NULL;
    if (_C.filename == NULL) return;

    char *ext = strrchr(_C.filename, '.');

    for (unsigned int j = 0; j < HLDB_ENTRIES; j++) {
        struct editorSyntax *s = &HLDB[j];
        unsigned int i = 0;
        while(s->filematch[i]) {
            int is_ext = (s->filematch[i][0] == '.');
            if ((is_ext && ext && !strcmp(ext, s->filematch[i])) ||
                (!is_ext && strstr(_C.filename, s->filematch[i]))) {
                _C.syntax = s;

                int filerow;
                for (filerow = 0; filerow < _C.numrows; filerow++) {
                    editorUpdateSyntax(&_C.row[filerow]);
                }
                return;
            }
            i++;
        }
    }
}