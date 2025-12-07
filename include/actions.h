#ifndef ACTIONS_H
#define ACTIONS_H

int action_stop(int pid);
int action_continue_(int pid);
int action_term(int pid);
int action_kill_(int pid);

void action_restart(pid_t pid);

#endif
