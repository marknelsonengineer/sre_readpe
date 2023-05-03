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

CC     = g++
CFLAGS = -Wall -Wextra $(DEBUG_CFLAGS) -std=c++17 -g -O0

SRCS = readpe.cpp

OBJS = $(SRCS:.cpp=.o)

MAIN = readpe

all: $(MAIN)

$(MAIN): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(MAIN)

%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

doc: $(TARGET)
	doxygen .doxygen/Doxyfile

test: $(MAIN)
	./$(MAIN) catnap64.exe

clean:
	rm -f $(OBJS) $(MAIN)
