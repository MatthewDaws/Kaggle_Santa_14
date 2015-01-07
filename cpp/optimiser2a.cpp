/** optimiser2a.cpp
 *
 *  Load in the output of optimiser2 and get the elves working on it.
 */
 
#include "elf_work.h"
using namespace Elfns;
#include "tasks.h"
#include "load_npy.h"
#include "save_npy.h"

#include <iostream>
using std::cout;
using std::endl;
#include <algorithm>
#include <vector>
#include <set>
#include <stdexcept>
#include <queue>

// ======================================================================================

/** Class to store all elves.  Currently handles which elves are still active, and
 *  finding the next free elf.
 */
class All_Elves {
public:
	All_Elves(const int num, Logger* ourlog=nullptr);
	Elf_Work& next_available_elf();
	void finished(const Elf_Work& e);
	int active_elves()const { return num_working_elves; }
	std::vector<Elf_Work>& get_list_elves() { return the_elves; }
private:
	std::vector<Elf_Work> the_elves;
	class working_elves_comp {
	public:
		working_elves_comp(std::vector<Elf_Work>& ptr) : the_elves_ptr{ptr} {}
		bool operator() (const int& lhs, const int& rhs)const
		{
			return the_elves_ptr[lhs].next_available() > the_elves_ptr[rhs].next_available();
		}
	private:
		std::vector<Elf_Work>& the_elves_ptr;
	};
	working_elves_comp pqc;
	std::priority_queue<int, std::vector<int>, working_elves_comp> working_elves;
	std::vector<bool> working;
	std::vector<int> waiting_list;
	int num_working_elves;
};

All_Elves::All_Elves(const int num, Logger* ourlog)
	: pqc{the_elves}, working_elves(pqc)
{
	for (int id=1; id<=num; ++id) {
		the_elves.push_back( Elf_Work{id, ourlog} );
		working_elves.push( id-1 );
		working.push_back( true );
	}
	num_working_elves = num;
}

Elf_Work& All_Elves::next_available_elf()
{
	//cout << "All_Elves::next_available_elf() --> ";
	// Add back any elves in waiting_list
	while ( waiting_list.size() > 0 ) {
		working_elves.push( waiting_list.back() );
		waiting_list.pop_back();
	}
	// Find elf to work, checking not `false` in `working`.
	int index;
	do {
		index = working_elves.top();
		working_elves.pop();
	} while ( working[ index ] == false );
	// Add to waiting_list
	waiting_list.push_back( index );
	//cout << "Elf returned: id=" << the_elves[index].get_id();
	//cout << " available=" << the_elves[index].next_available() << "\n";
	return the_elves[index];
}

/** Mark `elf_to_find` as not working */
void All_Elves::finished(const Elf_Work& elf_to_find)
{
	// We assume working[n] corresponds to elf with id n+1
	if ( working[ elf_to_find.get_id()-1 ] ) {
		--num_working_elves;
		working[ elf_to_find.get_id()-1 ] = false;
	}
}


// ============================================================================

struct AssembledTasks {
	struct opt2_out { int id, len; };
	std::vector<opt2_out> warmups;
	opt2_out longtask;
	bool operator<(const AssembledTasks& rhs) { return this->longtask.len < rhs.longtask.len; }
};

std::vector<AssembledTasks> load_opt2_output(std::string filename)
{
	auto opt2 = load_numpy_structured_rows<AssembledTasks::opt2_out>(filename, 2, "i4");
	// Assume gap is around 2000
	constexpr int longlength = 2000;
	std::vector<AssembledTasks> ret;
	auto bit = opt2.begin();
	while ( bit != opt2.end() ) {
		auto bend = bit;
		while ( bend != opt2.end() and bend->len < longlength ) { ++bend; }
		if ( bend == opt2.end() ) {
			throw std::runtime_error("load_opt2_output:: Unexpected format!");
		}
		AssembledTasks at;
		at.warmups = std::vector<AssembledTasks::opt2_out>(bit, bend);
		at.longtask = *bend;
		ret.push_back( at );
		++bend; bit = bend; // Next loop
	}
	return ret;
}

/** Various different algorithms to try.
 *  All run the warmup tasks on the actual elf.
 *
 *  This version returns the actual minutes wasted, and total finish time.
 */
std::pair<int,int> run_warmups_inorder(Elf_Work_Base &e, const AssembledTasks& task)
{
	int wasted = 0;
	for (auto wit = task.warmups.begin(); wit != task.warmups.end(); ++wit) {
		e.move_to_next_san_hour();
		int maxtask = e.max_task_in_san_hours();
		if ( maxtask < wit->len ) {
			wasted += dayends - ( e.next_available() % minsday );
			e.skip_to_start_day();
		}
		e.do_task(wit->len);
	}
	int etotaltime = e.next_available() + e.raw_time_for_task(task.longtask.len);
	return std::pair<int,int>(wasted, etotaltime);
}

/** This version allows re-ordering.  Returns the total time. */
int run_warmups_reorder(Elf_Work_Base &e, const AssembledTasks& task)
{
	std::vector<int> warmups(task.warmups.size());
	for (auto x : task.warmups) { warmups.push_back(x.len); }
	int catch_double = 0;
	while ( warmups.size() > 0 ) {
		e.move_to_next_san_hour();
		int maxtask = e.max_task_in_san_hours();
		auto bestit = warmups.begin();
		int best = -1;
		for (auto it = warmups.begin(); it != warmups.end(); ++it) {
			if ( *it <= maxtask and *it > best ) { best = *it; bestit = it; }
		}
		if ( best > -1 or catch_double >= 1 ) { // Found task we can do, so do it.
			e.do_task(best); warmups.erase(bestit);
			catch_double = 0;
		} else {
			++catch_double;
			e.skip_to_start_day();
		}
	}
	return e.next_available() + e.raw_time_for_task(task.longtask.len);
}

/** Run the elf `e` through the warmup tasks in `task`.
 *  Returns the number of wasted minutes.
 *
 *  TODO: Allow some wiggle room (e.g. 1 or 2 minutes into unsanctioned time)
 *        and/or reordering.
 */
int run_warmups(Elf_Work_Base &e, const AssembledTasks& task)
{
	auto pair = run_warmups_inorder(e, task);
	return pair.first;
	/*Elf_Work_Base newe = e;
	auto pair = run_warmups_inorder(newe, task);
	int wasted = pair.first;
	int order = pair.second;
	newe = e;
	int reorder = run_warmups_reorder(newe, task);
	if ( order <= reorder ) {
		run_warmups_inorder(e, task);
		return wasted;
	}
	run_warmups_reorder(e, task);
	return std::max(wasted + (reorder - order), 0);*/
}



// ============================================================================
/** Pack two ints into 32 bits, to save memory.
  */

/*class pq_data {
public:
	pq_data() : pq_data(0,0) {}
	pq_data(const int index_, const int wasted_) : mindex(index_), mwasted(wasted_) {}
	void set_index(const int index_) { mindex = index_; }
	void set_wasted(const int wasted_) { mwasted = wasted_; }
	int index()const { return mindex; }
	int wasted()const { return mwasted; }
	bool operator< (const pq_data& rhs)const { return this->wasted() > rhs.wasted(); }
private:
	int mindex, mwasted;
};*/

class pq_data {
public:
	pq_data() : pq_data(0,0) {}
	pq_data(const int index_, const int wasted_);
	void set_index(const int index);
	void set_wasted(const int wasted);
	int index()const;
	int wasted()const;
	bool operator< (const pq_data& rhs)const { return this->wasted() > rhs.wasted(); }
private:
	unsigned int store;
	static constexpr unsigned int indexsize = 21;
	static constexpr unsigned int indexmask = (1 << indexsize) - 1;
	static constexpr unsigned int wastedmax = (1 << (32 - indexsize)) - 1;
	static constexpr unsigned int wastedmask = wastedmax << indexsize;
};

pq_data::pq_data(const int index_, const int wasted_)
{
	set_index(index_); set_wasted(wasted_);
}

void pq_data::set_index(const int index)
{
	if ( index < 0 ) {
		throw std::runtime_error("pq_data::set_index negative.");
	}
	if ( static_cast<unsigned int>(index) > indexmask ) {
		cout << index << endl;
		throw std::runtime_error("pq_data::set_index too large.");
	}
	store = ( store & wastedmask ) + ( index & indexmask );
}

void pq_data::set_wasted(const int wasted)
{
	if ( wasted < 0 ) {
		throw std::runtime_error("pq_data::set_wasted negative.");
	}
	int w = wasted;
	if ( static_cast<unsigned int>(wasted) > wastedmax ) {
		w = wastedmax;
		//cout << wasted << endl;
		//throw std::runtime_error("pq_data::set_wasted too large.");
	}
	store = ( store & indexmask ) + ( w << indexsize );
}

int pq_data::index()const
{
	return store & indexmask;
}

int pq_data::wasted()const
{
	return store >> indexsize;
}


using pqtype = std::priority_queue<pq_data>;

pqtype make_pq(int mps, const std::vector<AssembledTasks>& tasks)
{
	Elf_Work_Base elf;
	elf.set_prod(0.25);
	elf.set_available_time(daystarts + mps);
	std::vector<pq_data> wastedlist_index;
	wastedlist_index.reserve( tasks.size() );
	int index = 0;
	for (const auto& at : tasks) {
		Elf_Work_Base e = elf;
		wastedlist_index.push_back( pq_data{index, run_warmups(e, at)} );
		++index;
	}
	return pqtype(wastedlist_index.begin(),wastedlist_index.end());
}

/** The more complicated algorithm, which uses priority queues to quickly(ish) find the
 *  task which best packs into the time available (as the elf may become free in the middle
 *  of the day).
 */
void algorithm(All_Elves& our_elves, std::vector<AssembledTasks>& tasks)
{
	std::vector<bool> already_done(tasks.size(), false);
	std::map<int,pqtype> mins_past_start;

	int total_tasks = tasks.size();
	int tasksdone = 0;
	while ( tasksdone < total_tasks ) {
		// Get next elf
		auto& elf = our_elves.next_available_elf();
		// Which priority queue to look at?
		auto tasksit = tasks.begin();
		elf.move_to_next_san_hour();
		int from_start = (elf.next_available() % minsday) - daystarts;
		if ( from_start < 0 or from_start >= sanctionedminsday ) {
			throw std::runtime_error("from_start out of bounds!");
		}
		if ( mins_past_start.find(from_start) == mins_past_start.end() ) {
			cout << "Building... " << mins_past_start.size() << "  \r";
			mins_past_start[ from_start ] = make_pq(from_start, tasks);
		}
		auto& pq = mins_past_start[ from_start ];
		// Find task (least wasted time) which hasn't already been done.
		pq_data which;
		do {
			which = pq.top(); pq.pop();
		} while ( already_done[which.index()] );
		already_done[which.index()] = true;
		tasksit += which.index();
		//cout << "Elf: " << elf.get_id() << " running warmups for long task " << tasksit->longtask.len;
		//cout << " which'll waste " << which.wasted() << " mins.\n";
		// Do tasksit
		run_warmups(elf, *tasksit);
		elf.do_task(tasksit->longtask.len);
		++tasksdone;
		if ( tasksdone % 1000 == 0 ) { cout << tasksdone << "/" << total_tasks << "   \r"; }
	}
}

/** This version just takes the next available elf, and skips to the next day as needed. */
void naive(All_Elves& our_elves, std::vector<AssembledTasks>& tasks)
{
	cout << "Running the naive algorithm" << endl;
	unsigned long long wasted = 0;
	for (const auto& task : tasks) {
		auto& elf = our_elves.next_available_elf();

		for (const auto& t : task.warmups) {
			elf.move_to_next_san_hour();
			if ( elf.max_task_in_san_hours() < t.len ) {
				wasted += dayends - ( elf.next_available() % minsday );
				elf.skip_to_start_day();
			}
			elf.do_task(t.len);
		}
		elf.do_task(task.longtask.len);
	}
	cout << "Total skipped (for all elves!) sanctioned minutes: " << wasted << "\n";
}

int main()
{
	auto tasks = load_opt2_output("opt2.npy");
	std::sort(tasks.begin(), tasks.end());
	cout << "Loaded " << tasks.size() << " tasks.\n";

	Logger ourlog;
	All_Elves our_elves(900, &ourlog);
	// Move all to 0.25 prod.
	for (auto& elf : our_elves.get_list_elves()) { elf.set_prod(0.25); }

	//algorithm(our_elves, tasks);
	naive(our_elves, tasks);
	ourlog.save_as_npy("optimiser2a_naive.npy");

	return 0;
}