/*** includes ***/
#include "include/term.hpp"

/*** defines ***/
#define CTRL_KEY(key) ((key) & 0x1f)

/*** usings ***/
using namespace std;

/*** data ***/
struct termios Term::orig_term = {};

/*** init ***/
Term::Term() {
    enableRawMode();
}
Term::~Term() {
    disableRawMode();
}

/*** methods ***/
void Term::disableRawMode() {
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_term) == -1) die("tcsetattr");
}

void Term::enableRawMode() {
    if (tcgetattr(STDIN_FILENO, &orig_term) == -1) die("tcgetattr");
    atexit(disableRawModeStatic);

    struct termios raw = orig_term;
    raw.c_iflag &= ~(BRKINT | INPCK | ISTRIP |IXON | ICRNL);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}


void Term::die(const char *msg) {
    perror(msg);
    exit(1);
}

void Term::readInput() {
    while (true) {
        char c = '\0';
        if(read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN) die("read");
        if (iscntrl(c))
        {
            cout << static_cast<int>(c) << "\r\n";
        }
        else
        {
            cout << static_cast<int>(c) << " ('" << c << "')" << "\r\n";
        }
        if (c == CTRL_KEY('q')) break;
    }
}

void Term::disableRawModeStatic() {
    if (tcgetattr(STDIN_FILENO, &orig_term) == -1) return; 
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_term) == -1) return;
}