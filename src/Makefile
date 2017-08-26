

OPTIMIZATION?=-O2
OPT=$(OPTIMIZATION)

STD=-std=c99 -pedantic
WARN=-Wall -Werror -W -Wno-unused-parameter

FINAL_CFLAGS=$(STD) $(WARN) $(OPT) $(CFLAGS)
FINAL_LDFLAGS=$(LDFLAGS)
FINAL_LIBS=-lm -luv

CCCOLOR="\033[34m"
LINKCOLOR="\033[34;1m"
SRCCOLOR="\033[33m"
BINCOLOR="\033[37;1m"
MAKECOLOR="\033[32;1m"
ENDCOLOR="\033[0m"

WR_CC=$(QUIET_CC)$(CC) $(FINAL_CFLAGS)
WR_LD=$(QUIET_LINK)$(CC) $(FINAL_LDFLAGS)

ifndef V
QUIET_CC = @printf '    %b %b\n' $(CCCOLOR)CC$(ENDCOLOR) $(SRCCOLOR)$@$(ENDCOLOR) 1>&2;
QUIET_LINK = @printf '    %b %b\n' $(LINKCOLOR)LINK$(ENDCOLOR) $(BINCOLOR)$@$(ENDCOLOR) 1>&2;
QUIET_INSTALL = @printf '    %b %b\n' $(LINKCOLOR)INSTALL$(ENDCOLOR) $(BINCOLOR)$@$(ENDCOLOR) 1>&2;
endif


WR_SERVER_NAME=white-rabbit-server
WR_SERVER_OBJ=wheel.o server.o

all: $(WR_SERVER_NAME)

.PHONY: all

# white-rabbit server
$(WR_SERVER_NAME): $(WR_SERVER_OBJ)
	$(WR_LD) $(OPTIMIZATION) $(INCLUDES) $(LD_PATH) -o $@ $^ $(FINAL_LIBS)

%.o: %.c
	$(WR_CC) -c $<

clean:
	rm -rf $(WR_SERVER_NAME) *.o
