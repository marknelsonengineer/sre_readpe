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

# Compiler and flags
CC           = g++
DEBUG_CFLAGS = -g -DDEBUG
CFLAGS       = -Wall -Wextra $(DEBUG_CFLAGS) -std=c++17

# Source files
SRCS = readpe.cpp

# Object files
OBJS = $(SRCS:.cpp=.o)

# Executable
MAIN = readpe

# Targets
all: $(MAIN)

$(MAIN): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(MAIN)

%.o: %.cpp %.h
	$(CC) $(CFLAGS) -c $< -o $@

# Test
test: $(MAIN)
	./$(MAIN) catnap64.exe

clean:
	rm -f $(OBJS) $(MAIN)

# Debug
debug: CFLAGS = $(DEBUG_CFLAGS)
debug: $(MAIN)
	./$(MAIN) catnap64.exe

doc: $(TARGET)
	doxygen .doxygen/Doxyfile
