PROG?=		PlistBuddy

SRCS=		main.c \
		State.c \
		PlistIO.c \
		Parse.c \
		Entries.c \
		Print.c \
		Commands.c
HDRS=		PlistBuddy.h
TARGET?=	${PROG}
OBJS=		main.o \
		State.o \
		PlistIO.o \
		Parse.o \
		Entries.o \
		Print.o \
		Commands.o

CFLAGS?=	-g -O2
WARNFLAGS?=	-Wall -Wextra -Wformat
COMPILE_FLAGS=	${CPPFLAGS} ${CFLAGS} ${WARNFLAGS}
LINK_FLAGS=	${LDFLAGS}
LDADD?=		-framework CoreFoundation

.PHONY: all check check-diff clean show-config update-reference

all: ${TARGET}

check: ${TARGET}
	OURS='${TARGET}' sh ./tests/regression.sh

check-diff: ${TARGET}
	OURS='${TARGET}' sh ./tests/check-diff.sh

update-reference:
	SOURCE='/usr/libexec/PlistBuddy' sh ./tests/update-reference.sh

show-config:
	@printf 'PROG=%s\n' '${PROG}'
	@printf 'CC=%s\n' '${CC}'
	@printf 'CPPFLAGS=%s\n' '${CPPFLAGS}'
	@printf 'CFLAGS=%s\n' '${CFLAGS}'
	@printf 'LDFLAGS=%s\n' '${LDFLAGS}'
	@printf 'TARGET=%s\n' '${TARGET}'

${TARGET}: ${OBJS}
	${CC} ${LINK_FLAGS} -o $@ ${OBJS} ${LDADD}

main.o: main.c ${HDRS}
	${CC} ${COMPILE_FLAGS} -c main.c -o $@

State.o: State.c ${HDRS}
	${CC} ${COMPILE_FLAGS} -c State.c -o $@

PlistIO.o: PlistIO.c ${HDRS}
	${CC} ${COMPILE_FLAGS} -c PlistIO.c -o $@

Parse.o: Parse.c ${HDRS}
	${CC} ${COMPILE_FLAGS} -c Parse.c -o $@

Entries.o: Entries.c ${HDRS}
	${CC} ${COMPILE_FLAGS} -c Entries.c -o $@

Print.o: Print.c ${HDRS}
	${CC} ${COMPILE_FLAGS} -c Print.c -o $@

Commands.o: Commands.c ${HDRS}
	${CC} ${COMPILE_FLAGS} -c Commands.c -o $@

clean:
	rm -f ${OBJS} ${TARGET}
