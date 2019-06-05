INCLUDES  = -I$(MACHBASE_HOME)/include
LIBDIRS   = -L$(MACHBASE_HOME)/lib
LIBS      = -lmachbasecli -lm -lpthread -ldl -lrt
BINARIES  = measure retrieve

all: $(BINARIES)

measure: measure.c
	gcc -O2 -g -o measure measure.c $(INCLUDES) $(LIBDIRS) $(LIBS)

retrieve: retrieve.c
	gcc -O2 -g -o retrieve retrieve.c $(INCLUDES) $(LIBDIRS) $(LIBS)

clean:
	rm -Rf $(BINARIES)
