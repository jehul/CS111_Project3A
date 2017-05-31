SOURCE=lab3a.c
EXEC=lab3a
SANEFILE=P3A_check.sh
STID=004569045

build: lab3a

lab3a: lab3a.c
	gcc -g $(SOURCE) -o $(EXEC)

clean:
	@-rm $(EXEC) lab3a-$(STID).tar.gz

dist:
	tar -zcvf lab3a-$(STID).tar.gz $(SOURCE) Makefile README ext2_fs.h

sane:
	./$(SANEFILE) $(STID)
