/*** includes ***/
#include "include/term.hpp"

/*** defines ***/
#define CTRL_KEY(key) ((key) & 0x1f)

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

char Term::editorReadKey() {
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) die("read");
    }
    return c;
}

bool Term::editorProccessKeypress() {
    char c = editorReadKey();

    switch(c) {
        case CTRL_KEY('q'):
            return false;
    }
    return true;
}

void Term::editorRefreshScreen() {
    string ab;

    ab += "\x1b[2J";
    ab += "\x1b[H";
    
    editorDrawRows(ab);
    ab += "\x1b[H";
    write(STDOUT_FILENO, ab.c_str(), ab.length());
}

void Term::editorDrawRows(string &ab) {
    for (int y = 0; y < config_.screen_rows; y++) {
        ab += '~';

        if(y < config_.screen_rows - 1) {
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
    if (getWindowSize(&config_.screen_rows, &config_.screen_cols) == -1) {
        die("getWindowSize");
    }
}