# don't touch below this line

LEXSRCS := cmdparse.lex.c cmdparse.tab.h

all:	csources

%.lex.c: %.l %.tab.h
# The ideal size for the flex buffer is the length of the longest token expected, in bytes, plus a little more.
	flex -DYY_BUF_SIZE=32 -o$@ $<

%.tab.h %.tab.c: %.y
	bison -Wno-other -d $<


csources: $(LEXSRCS)

clean:
	$(RM) *.output *.tab.h *.tab.c *.lex.c *.o
