#include <ncurses.h>
#include "process.h"

void draw_process_table(process_node* list, int selected) {
    int row = 2;
    int index = 0;

    // En-tête du tableau
    attron(A_BOLD);
    mvprintw(row++, 0, " PID     USER       MEM(KB)     STATE        NAME");
    attroff(A_BOLD);

    process_node* node = list;
    while (node) {

        // Highlight si sélectionné
        if (index == selected) {
            attron(A_REVERSE);
        }

        mvprintw(row, 0, "%5d   %-10s  %8d   %-10s   %s",
            node->process.pid,
            node->process.user,
            node->process.memory,
            node->process.state,
            node->process.name
        );

        if (index == selected) {
            attroff(A_REVERSE);
        }

        node = node->next;
        row++;
        index++;
    }
}
