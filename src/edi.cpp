#include "include/term.hpp"

int main() {
    Term term;
    term.initEditor();
    
    while (true) {
        term.editorRefreshScreen();
        if (term.editorProccessKeypress() == false) break;
    }
    
    return 0;
}