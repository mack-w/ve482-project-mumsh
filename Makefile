mumsh: main.o parse.o execute.o jobs.o pwd.o cd.o
	cc 	 -o mumsh main.o parse.o execute.o jobs.o pwd.o cd.o
install:
	@echo "Are you serious?"
clean:
	rm -f *.o
	rm -f mumsh
