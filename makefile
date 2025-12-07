CC = gcc
CFLAGS = -Iinclude
LIBS = -lncurses

SRC = \
    src/main.c \
    src/ui/ui.c \
    src/ui/ui_helpers.c \
    src/actions/actions.c \
    src/actions/restart.c \
    src/process_manager/process_manager.c \
    src/process_manager/process_reader.c \
    src/process_manager/cpu_history.c \
    src/process_manager/list_utils.c

OBJ = $(SRC:.c=.o)

lp25htop: $(OBJ)
	$(CC) $(OBJ) -o lp25htop $(LIBS)

clean:
	rm -f $(OBJ) lp25htop
