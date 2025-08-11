#include "include/edi.hpp"

using namespace std;

struct termios orig_term;

int main() {
    enableRawMode();

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
        if (c == 'q') break;
    }
    return 0;
}

void disableRawMode() {
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_term) == -1) die("tcsetattr");
}

void enableRawMode() {
    if (tcgetattr(STDIN_FILENO, &orig_term) == -1) die("tcgetattr");
    atexit(disableRawMode);

    struct termios raw = orig_term;
    raw.c_iflag &= ~(BRKINT | INPCK | ISTRIP |IXON | ICRNL);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}


void die(const char *s) {
    perror(s);
    exit(1);
}