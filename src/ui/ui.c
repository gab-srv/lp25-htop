#include <ncurses.h>
#include <unistd.h>
#include "../../include/process.h"
#include "../../include/process_manager.h"
#include "../../include/actions.h"

void ui_run()
{
    process_node *head = NULL;
    int selected = 0;
    int count = 0;

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    timeout(0);

    while (1){

        if (head)
            free_process_list(head);

        // Récupère top 5 processus
        head = get_local_process_list(5);

        clear();

        mvprintw(0, 0, "LP25htop | ↑ ↓ navigate | F5=stop F6=cont F7=term F8=kill | q quit");
        mvprintw(2, 0, "PID   USER        CPU%%   MEM(KB)   STATE      NAME");

        int row = 3;
        count = 0;
        process_node *cur = head;

        while (cur){
            if (count == selected) attron(A_REVERSE);

            mvprintw(row, 0, "%-5d %-10s %6.2f %8d %-10s %s",
                     cur->process.pid,
                     cur->process.user,
                     cur->process.cpu_percent,
                     cur->process.memory_kb,
                     cur->process.state,
                     cur->process.name);

            if (count == selected) attroff(A_REVERSE);

            row++; count++;
            cur = cur->next;
        }

        int ch = getch();

        if (ch == 'q') break;
        else if (ch == KEY_UP && selected > 0) selected--;
        else if (ch == KEY_DOWN && selected < count-1) selected++;

        else if (ch == KEY_F(5) || ch == KEY_F(6) || ch == KEY_F(7) || ch == KEY_F(8)){
            int i = 0;
            cur = head;

            while (cur && i < selected){ cur = cur->next; i++; }
            if (!cur) continue;

            int pid = cur->process.pid;

            if (ch == KEY_F(5)) action_stop(pid);
            if (ch == KEY_F(6)) action_continue_(pid);
            if (ch == KEY_F(7)) action_term(pid);
            if (ch == KEY_F(8)) action_kill_(pid);
        }

        refresh();
        sleep(1);
    }

    endwin();
    free_process_list(head);
}
