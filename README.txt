===============================
README
===============================

CSE430 - Operating Systems
Project 2 Solution
Author: Vageesh Bhasin

===============================
1. Run 'make all' to generate the executable 'project_2.exe'
2. The executable require the file name with location as an input parameter.
Example:
./project_2 /home/CSE430_OS/Project_2/Input/par.cc
3. The output file will be generated within the same folder with a postfix '_construct' and file extension '.cc'
4. Compile and run the file with -pthread flag.
Example:
Compile: g++ -std=c++11 -c -pthread par_construct.cc
Executable: g++ -std=c++11 -o -pthread par_construct par_construct.o
Run: ./par_construct



