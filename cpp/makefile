CC = g++
//CPPFLAGS = -Wall -std=c++11 -D_GLIBCXX_DEBUG
//CPPFLAGS = -Wall -std=c++11 
CPPFLAGS = -Wall -std=c++11 -O2 -march=native -mtune=native

scorer.exe : scorer.o load_npy.o calendar.o load_toys.o elf.o
	g++ $(CPPFLAGS) -mconsole $^ -o $@

optimiser5.exe : optimiser5.o load_npy.o calendar.o save_npy.o
	g++ $(CPPFLAGS) -mconsole $^ -o $@

optimiser4.exe : optimiser4.o load_npy.o save_npy.o elf.o tasks.o elf_work.o
	g++ $(CPPFLAGS) -mconsole $^ -o $@

optimiser2a.exe : optimiser2a.o load_npy.o save_npy.o elf.o tasks.o elf_work.o
	g++ $(CPPFLAGS) -mconsole $^ -o $@

optimiser2.exe : optimiser2.o load_npy.o save_npy.o elf.o tasks.o elf_work.o
	g++ $(CPPFLAGS) -mconsole $^ -o $@

optimiser3.exe : optimiser3.o load_npy.o save_npy.o elf.o tasks.o elf_work.o
	g++ $(CPPFLAGS) -mconsole $^ -o $@

optimiser.exe : optimiser.o load_npy.o save_npy.o elf.o tasks.o elf_work.o
	g++ $(CPPFLAGS) -mconsole $^ -o $@

calendar.o : calendar.cpp calendar.h
	g++ -c $(CPPFLAGS) $< -o $@

load_npy.o : load_npy.cpp load_npy.h
	g++ -c $(CPPFLAGS) $< -o $@

save_npy.o : save_npy.cpp save_npy.h
	g++ -c $(CPPFLAGS) $< -o $@

elf.o : elf.cpp elf.h
	g++ -c $(CPPFLAGS) $< -o $@

optimiser2a.o : optimiser2a.cpp elf.h load_npy.h elf_work.h
	g++ -c $(CPPFLAGS) $< -o $@

optimiser.o : optimiser.cpp elf.o load_npy.o elf_work.o
	g++ -c $(CPPFLAGS) $< -o $@

tasks.o : tasks.cpp tasks.h
	g++ -c $(CPPFLAGS) $< -o $@

elf_work.o : elf_work.cpp elf_work.h
	g++ -c $(CPPFLAGS) $< -o $@

clean:
	rm optimiser.o optimiser2.o elf.o load_npy.o save_npy.o calendar.o optimiser.exe tasks.o elf_work.o
