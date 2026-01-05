#include <ncurses.h>
#include <unistd.h>
#include <string.h>
#include "../include/process.h"
#include "../include/actions.h"
#include "../include/client.h"
#include <stdio.h>
#include <stdlib.h>

static void truncate_with_ellipsis(char *dst, const char *src, int width)
{
    if (width <= 0) { if (dst) dst[0] = '\0'; return; }
    int src_len = (int)strlen(src);
    if (src_len <= width) {
        strncpy(dst, src, width);
        dst[width] = '\0';
        return;
    }
    if (width <= 3) {
        strncpy(dst, src, width);
        dst[width] = '\0';
        return;
    }
    int keep = width - 3;
    strncpy(dst, src, keep);
    dst[keep] = '\0';
    strcat(dst, "...");
}

static void print_row_columns(int row, int start_col,
                              const char *pid_field, int pid_w,
                              const char *user_field, int user_w,
                              const char *cpu_field, int cpu_w,
                              const char *mem_field, int mem_w,
                              const char *state_field, int state_w,
                              const char *name_field)
{
    int col = start_col;
    mvprintw(row, col, "%*s", pid_w, pid_field); col += pid_w + 1;
    mvprintw(row, col, "%-*s", user_w, user_field); col += user_w + 1;
    mvprintw(row, col, "%*s", cpu_w, cpu_field); col += cpu_w + 1;
    mvprintw(row, col, "%*s", mem_w, mem_field); col += mem_w + 1;
    mvprintw(row, col, "%-*s", state_w, state_field); col += state_w + 1;
    mvprintw(row, col, "%s", name_field);
}

void ui_run()
{
    int selected = 0;

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    timeout(0);

    while (1){
        int rows, cols;
        getmaxyx(stdscr, rows, cols);

        process_t processes[128];
        int count = fetch_processes(processes, 128);

        if (selected >= count)
            selected = count - 1;
        if (selected < 0)
            selected = 0;

        clear();

        mvprintw(0, 0, "LP25htop | ↑ ↓ navigate | F5=stop F6=cont F7=term F8=kill | q quit");

        const int pid_w = 6;    /* espace pour PID */
        const int user_w = 14;  /* nom utilisateur */
        const int cpu_w = 6;    /* CPU% */
        const int mem_w = 9;    /* MEM(KB) */
        const int state_w = 10; /* STATE */
        const int fixed_margin = 5; /* espaces inter-colonnes additionnels */

        int used = pid_w + user_w + cpu_w + mem_w + state_w;
        int name_w = cols - used - fixed_margin - 1;
        if (name_w < 10) name_w = 10;

        char hdr_pid[32], hdr_user[32], hdr_cpu[32], hdr_mem[32], hdr_state[32];
        snprintf(hdr_pid, sizeof(hdr_pid), "PID");
        snprintf(hdr_user, sizeof(hdr_user), "USER");
        snprintf(hdr_cpu, sizeof(hdr_cpu), "CPU%%");
        snprintf(hdr_mem, sizeof(hdr_mem), "MEM(KB)");
        snprintf(hdr_state, sizeof(hdr_state), "STATE");

        print_row_columns(2, 0,
                          hdr_pid, pid_w,
                          hdr_user, user_w,
                          hdr_cpu, cpu_w,
                          hdr_mem, mem_w,
                          hdr_state, state_w,
                          "NAME");

        int row = 3;
        char pid_field[32], user_field[64], cpu_field[32], mem_field[32], state_field[32], name_field[512], name_trunc[512];

        for (int i = 0; i < count && row < rows - 1; i++, row++) {
            if (i == selected) attron(A_REVERSE);

            snprintf(pid_field, sizeof(pid_field), "%d", processes[i].pid);
            snprintf(user_field, sizeof(user_field), "%s", processes[i].user);
            snprintf(cpu_field, sizeof(cpu_field), "%0.2f", processes[i].cpu_percent);
            snprintf(mem_field, sizeof(mem_field), "%d", processes[i].memory_kb);
            snprintf(state_field, sizeof(state_field), "%s", processes[i].state);

            truncate_with_ellipsis(name_trunc, processes[i].name, name_w);
            snprintf(name_field, sizeof(name_field), "%s", name_trunc);

            print_row_columns(row, 0,
                              pid_field, pid_w,
                              user_field, user_w,
                              cpu_field, cpu_w,
                              mem_field, mem_w,
                              state_field, state_w,
                              name_field);

            if (i == selected) attroff(A_REVERSE);
        }

        int ch = getch();

        if (ch == 'q') break;
        else if (ch == KEY_UP && selected > 0) selected--;
        else if (ch == KEY_DOWN && selected < count-1) selected++;

        else if (ch == KEY_F(5) || ch == KEY_F(6) || ch == KEY_F(7) || ch == KEY_F(8)){
            if (count > 0) {
                int pid = processes[selected].pid;

                if (ch == KEY_F(5)) action_stop(pid);
                if (ch == KEY_F(6)) action_continue_(pid);
                if (ch == KEY_F(7)) action_term(pid);
                if (ch == KEY_F(8)) action_kill_(pid);
            }
        }

        refresh();
        sleep(1);
    }

    endwin();
}