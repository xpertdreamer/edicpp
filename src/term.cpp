/*** defines ***/
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE
#define CTRL_KEY(key) ((key) & 0x1f)

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
            int times = config_.screen_rows;
            while (times--) {
                editorMoveCursor(c == CTRL_ARROW_UP ? ARROW_UP : ARROW_DOWN);
            }
            break;
        }
        case CTRL_ARROW_LEFT:
            // fall through
        case CTRL_ARROW_RIGHT:
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
    string ab;

    ab += "\x1b[?25l";
    ab += "\x1b[H";
    
    editorDrawRows(ab);

    char buffer[32];
    snprintf(buffer, sizeof(buffer), "\x1b[%d;%dH", config_.cursor_y + 1, config_.cursor_x + 1);
    ab += buffer;

    ab += "\x1b[?25h";

    write(STDOUT_FILENO, ab.c_str(), ab.length());
}

void Term::editorDrawRows(string &ab) {
    for (int y = 0; y < config_.screen_rows; y++) {
        if (y >= config_.numrows) {
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
            int len = config_.row.size;
            if (len > config_.screen_cols) len = config_.screen_cols;
            ab += config_.row.chars;
        }
            ab += "\x1b[K";
        if (y < config_.screen_rows - 1) {
            ab += "\r\n";
        }
    }
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
    config_.numrows = 0;
    config_.row = NULL;
    if (getWindowSize(&config_.screen_rows, &config_.screen_cols) == -1) {
        die("getWindowSize");
    }
}

void Term::editorMoveCursor(int key) {
    switch (key)
    {
    case ARROW_LEFT:
        if (config_.cursor_x > 0) {
            config_.cursor_x--;
        }
        break;
    case ARROW_RIGHT:
        if (config_.cursor_x != config_.screen_cols - 1) {
            config_.cursor_x++;
        }
        break;
    case ARROW_UP:
        if (config_.cursor_y > 0) {
            config_.cursor_y--;
        }
        break;
    case ARROW_DOWN:
        if (config_.cursor_y != config_.screen_rows - 1) {
            config_.cursor_y++;
        }
        break;
    case CTRL_ARROW_LEFT:
        config_.cursor_x = 0;
        break;
    case CTRL_ARROW_RIGHT:
        config_.cursor_x = config_.screen_cols - 1;
        break;
    }
}

void Term::editorOpen(char* filename) {
    FILE *file = fopen(filename, "r");
    if (!file) die("fopen");

    char *line = NULL;
    size_t lineCap = 0;
    ssize_t lineLen;
    lineLen = getline(&line, &lineCap, file);
    if (lineLen != -1) {
        while (lineLen > 0 && (line[lineLen - 1] == '\n' ||
                               line[lineLen - 1] == '\r')) {
            lineLen--;
        }
        config_.row.size = lineLen;
        config_.row.chars = (char*)malloc(lineLen + 1);
        memcpy(config_.row.chars, line, lineLen);
        config_.row.chars[lineLen] = '\0';
        config_.numrows = 1;
    }
    free(line);
    fclose(file);
}