PROGRAM         = depriv
OBJECTS         = $(PROGRAM).o
CC		= cc
CFLAGS		= 
LDFLAGS		=

all: $(PROGRAM)

$(PROGRAM): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $(PROGRAM)

%.o: %.c
	$(CC) -c $<

clean:
	rm -f $(PROGRAM) $(OBJECTS)

realclean distclean cleandir: clean

