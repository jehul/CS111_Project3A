SOURCE=lab3a.c
EXEC=lab3a
TARBALL=lab3a-*.tar.gz

build: lab3a

lab3a: lab3a.c
	gcc -g $(SOURCE) -o $(EXEC)

clean:
	@-rm $(EXEC) $(TARBALL)
