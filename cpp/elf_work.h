/** elf_work.h
 *
 *  Implements class Elf_Work which supports further functionality to the Elf class,
 *  specifically for assigning tasks and so forth.
 *
 *  Also includes a simple logging output, if required.
 */

#ifndef __elf_work_h
#define __elf_work_h

#include "elf.h"
#include "tasks.h"
#include <vector>

class Logger
{
public:
	Logger() {}
	void add_entry(const int elfid, const int tasklen, const int time, const int elfduration);
	void save_as_npy(std::string filename);
private:
	struct entry { int elfid, tasklen, time, elfduration; };
	std::vector<entry> log;
};

/** Store tasks: might develop this later */
class Elf_Work_Store {
public:
	Elf_Work_Store() {};
	int num_stored()const { return store.size(); }
	toy_task peek()const { return store.back(); }
	toy_task get() { auto r = store.back(); store.pop_back(); return r; }
	void put(const toy_task& t) { store.push_back(t); }
private:
	std::vector<toy_task> store;
};

/** Basic subclass to support extra functions in the optimiser. */
class Elf_Work_Base : public Elfns::Elf
{
public:
	Elf_Work_Base(const int i=0) : Elfns::Elf{i} {}
	void move_to_next_san_hour();
	void skip_to_start_day();
	int max_task_in_san_hours()const;
	int max_task_at_cur_prod(const int len)const;
	int raw_time_for_task(const int len)const;
	virtual int do_task(const int len);
	virtual int do_task(toy_task t);
	void set_prod(const double p);
	double get_prod()const { return productivity; }
	void set_available_time(const int time) { available_to_work = time; }
};

/** Further subclass to add: logging, recording last finish time, Elf_Work_Store */
class Elf_Work : public Elf_Work_Base
{
public:
	Elf_Work(const int i=0, Logger* logp=nullptr) : Elf_Work_Base{i}, logptr{logp}, last_finish_time(0) {}
	int max_task_if_in_san_hours();
	int task_length_to_attain_prod(const double target_prod)const;
	int do_task(toy_task t);
	int do_task(const int len);
	int get_id()const { return id; }
	int get_last_finish_time()const { return last_finish_time; }
public:
	Elf_Work_Store stored;
private:
	Logger* logptr;
	int last_finish_time;
};


#endif // __elf_work_h
