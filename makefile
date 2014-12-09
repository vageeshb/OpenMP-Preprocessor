################################################################################
# Project 2 Make file - Created by Vageesh Bhasin
################################################################################
all: Project_2

clean:
	rm -rf main.o Project_2

Project_2: main.o
	@echo 'Generating executable'
	g++ -std=c++11 -g -o  Project_2 ./Output/main.o

main.o:
	@echo 'Compiling program'
	g++ -std=c++11 -c -g ./src/main.cc
	if [ -d "./Output" ]; then echo "Output dir exists"; else mkdir ./Output; fi
	@echo 'Moving object files to Output folder'
	mv *.o ./Output/
