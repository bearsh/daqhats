NAME = single_value_read
OBJ = $(NAME).o
LIBS = -ldaqhats -lgpiod -lm
CFLAGS = -Wall -I/usr/local/include -I./ -g
EXTENSION = .c

all: $(NAME)

%.o: %$(EXTENSION) $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(NAME): $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

.PHONY: clean

clean:
	@rm -f *.o *~ core $(NAME)
