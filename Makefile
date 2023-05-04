###############################################################################
###               University of Hawaii, College of Engineering
###               Lab 6 - readpe - SRE - Spring 2023
###
### PE File viewer
###
### @see     https://www.gnu.org/software/make/manual/make.html
###
### @file    Makefile
### @author  Thanh Ly thanhly@hawaii.edu>
### @author  Mark Nelson marknels@hawaii.edu>
###############################################################################

CC        = g++
CFLAGS    = -Wall -Wextra $(DEBUG_CFLAGS) -std=c++17 -g -O0
LINT      = clang-tidy
LINTFLAGS = --quiet --extra-arg-before=-std=c++17

valgrind: CFLAGS   += -DTESTING -g -O0 -fno-inline
valgrind: CXXFLAGS +=           -g -O0 -fno-inline -march=x86-64 -mtune=generic

SRCS = readpe.cpp

OBJS = $(SRCS:.cpp=.o)

MAIN = readpe

all: $(MAIN)

$(MAIN): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(MAIN)

%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

doc: $(MAIN)
	doxygen .doxygen/Doxyfile

test: $(MAIN)
	./$(MAIN) catnap64.exe

lint: $(MAIN)
	$(LINT) $(LINTFLAGS) $(SRCS) --

valgrind: $(MAIN)
	sudo DEBUGINFOD_URLS="https://debuginfod.archlinux.org"                    \
	valgrind --leak-check=full --track-origins=yes --error-exitcode=1 --quiet  \
	./$(MAIN) ./catnap32.exe

clean:
	rm -f $(OBJS) $(MAIN)
