/** optimiser4.cpp
 *
 *  Put it all together!
 *
 *  With optimiser2 run in the usual way, get 198,942,124
 *  With other algorithm, get 199,932,101
 */

#include "elf_work.h"
using namespace Elfns;
#include "tasks.h"
#include "save_npy.h"
#include "load_npy.h"

#include <iostream>
using std::cout;
using std::endl;
#include <cmath>
#include <algorithm>
#include <fstream>
#include <vector>
#include <map>
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
	int maximum_finish_time()const;
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

int All_Elves::maximum_finish_time()const
{
	auto comp = [](const Elf_Work& lhs, const Elf_Work& rhs)
			{ return lhs.get_last_finish_time() < lhs.get_last_finish_time(); };
	auto it = std::max_element(the_elves.begin(), the_elves.end(), comp);
	return it->get_last_finish_time();
}

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

/** Load the raw toys */
class Raw_Toys {
public:
	struct toy_data { int starttime, dur; };
	Raw_Toys(std::vector<toy_data> input);
	int peek_first_arrival(const int length)const;
	toy_task get_first_arrival(const int length);
	int size()const { return toys_left; }
	int do_task(Elf_Work& elf, const int len);
	int peek_best_available(const int maxlen, const int maxtime)const;
	int longest_task()const { return toys.rbegin()->first; }
	std::vector<int> get_list_lens()const;
private:
	struct toys_by_len {
		int id, starttime;
		bool operator<(const toys_by_len& rhs) { return this->starttime > rhs.starttime; }
	};
	std::map<int,std::vector<toys_by_len>> toys;
	int toys_left;
};

std::vector<int> Raw_Toys::get_list_lens()const
{
	std::vector<int> ret;
	for (auto it = toys.begin(); it != toys.end(); ++it) {
		for (std::size_t n = 0; n < it->second.size(); ++n) {
			ret.push_back( it->first );
		}
	}
	return ret;
}

Raw_Toys::Raw_Toys(std::vector<toy_data> input)
{
	toys_left = static_cast<int>(input.size());
	// Put into our map
	for (decltype(input.size()) index = 0; index < input.size(); ++index) {
		int id = static_cast<int>(index) + 1;
		auto t = input[index];
		toys[t.dur].push_back( toys_by_len{id, t.starttime} );
	}
	// Order each entry
	for (auto& pair : toys) {
		auto& vec = pair.second;
		std::sort(vec.begin(), vec.end());
	}
}

int Raw_Toys::peek_first_arrival(const int length)const
{
	auto it = toys.find(length);
	if ( it == toys.end() ) { return -1; }
	const auto t = it->second.back();
	return t.starttime;
}

/** Returns the largest task length which is <=maxlen and which arrives at or before maxtime
 *  or -1 if none. */
int Raw_Toys::peek_best_available(const int maxlen, const int maxtime)const
{
	// it will point to first element with key >= maxlen
	auto it = toys.lower_bound(maxlen);
	if ( it == toys.end() or it->first > maxlen ) { --it; }
	while ( true ) {
		auto& vec = it->second;
		if ( vec.back().starttime <= maxtime ) { return it->first; }
		if ( it == toys.begin() ) { break; }
		--it;
	}
	return -1;
}

toy_task Raw_Toys::get_first_arrival(const int length)
{
	auto it = toys.find(length);
	if ( it == toys.end() ) { throw std::runtime_error("Raw_Toys::get_first_arrival illegal length."); }
	--toys_left;
	auto* vec = &(it->second);
	auto t = vec->back();
	vec->pop_back();
	if ( vec->size() == 0 ) {
		vec = nullptr;
		toys.erase(length);
	}
	return toy_task{t.id, t.starttime, length};
}

/** Puts the `elf` to work on task of length `len`, but if not
 *  currently available, then force elf to wait.
 */
int Raw_Toys::do_task(Elf_Work& elf, const int len)
{
	if ( peek_first_arrival(len) == -1 ) {
		throw std::runtime_error("Raw_Toys::do_task no such task of required length.");
	}
	auto t = get_first_arrival(len);
	if ( t.arrival_time > elf.next_available() ) {
		// Round up to sanctioned hours
		int time = t.arrival_time;
		int local_time = time % minsday;
		if ( local_time >= dayends ) {
			time += minsday - local_time + daystarts;
		} else {
			if ( local_time < daystarts ) {
				time += daystarts - local_time;
			}
		}
		elf.set_available_time(time);
	}
	return elf.do_task(t);
}

/** Load in all toys into the Raw_Toys structure */
Raw_Toys load_raw_toys(std::string filename)
{
	auto tmp = load_numpy_structured_rows<Raw_Toys::toy_data>(filename, 2, "i4");
	return Raw_Toys(tmp);
}


// ============================================================================

/** Get the _lengths_ of the tasks we haven't otherwise handled. */
std::vector<int> other_task_lengths()
{
	task_buffer_map tasks;
	std::ifstream tasksin("tasks2.save", std::ios::binary);
	tasks.load(tasksin);
	std::vector<int> lengths;
	while ( tasks.remaining() > 0 ) {
		int len = tasks.find_largest_length();
		auto t = tasks.extract_task(len);
		lengths.push_back(t.len);
	}
	return lengths;
}



// ============================================================================
// Load tasks we've "packaged" using optimiser2

struct AssembledTasks {
	struct opt2_out { int id, len; };
	std::vector<opt2_out> warmups;
	opt2_out longtask;
	bool operator<(const AssembledTasks& rhs) { return this->longtask.len < rhs.longtask.len; }
};

std::vector<AssembledTasks> load_opt2_output(const std::string& filename)
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
std::pair<int,int> trial_warmups_inorder(Elf_Work_Base& e, const AssembledTasks& task, Raw_Toys* all_toys = nullptr)
{
	int wasted = 0;
	for (auto wit = task.warmups.begin(); wit != task.warmups.end(); ++wit) {
		e.move_to_next_san_hour();
		int maxtask = e.max_task_in_san_hours();
		if ( maxtask < wit->len ) {
			wasted += dayends - ( e.next_available() % minsday );
			e.skip_to_start_day();
		}
		if ( all_toys == nullptr ) {
			e.do_task(wit->len);
		} else {
			auto t = all_toys->get_first_arrival(wit->len);
			e.do_task(t);
		}
	}
	int etotaltime = e.next_available() + e.raw_time_for_task(task.longtask.len);
	return std::pair<int,int>(wasted, etotaltime);
}
/** Run the elf `e` through the warmup tasks in `task`.
 *  Returns the number of wasted minutes.
 *
 *  Currently commented out code tests reordering.
 *  TODO: Allow wiggle room of +2 mins into unsanctioned, or whatever?
 */
int trial_warmups(const Elf_Work_Base& ein, const AssembledTasks& task)
{
	Elf_Work_Base e = ein;
	auto pair = trial_warmups_inorder(e, task);
	return pair.first;
}

/** As above, but actually run on a production elf, using all_toys */
void run_warmups(Elf_Work &e, const AssembledTasks& task, Raw_Toys& all_toys)
{
	trial_warmups_inorder(e, task, &all_toys);
}



// ============================================================================
/** Pack two ints into 32 bits, to save memory.
  */

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
		wastedlist_index.push_back( pq_data{index, trial_warmups(elf, at)} );
		++index;
	}
	return pqtype(wastedlist_index.begin(),wastedlist_index.end());
}

/** Run the elves over the packaged tasks
 */
void do_packaged_tasks(All_Elves& our_elves, std::vector<AssembledTasks>& tasks, Raw_Toys& all_toys)
{
	std::vector<bool> already_done(tasks.size(), false);
	std::map<int,pqtype> mins_past_start;
	int tasksdone = 0;
	while ( tasksdone < static_cast<int>(tasks.size()) ) {
		// Get next elf
		auto& elf = our_elves.next_available_elf();
		// Special case if 1st task...
		auto tasksit = tasks.begin();
		// Which priority queue to look at?
		elf.move_to_next_san_hour();
		int from_start = (elf.next_available() % minsday) - daystarts;
		if ( mins_past_start.find(from_start) == mins_past_start.end() ) {
			cout << "Building... " << mins_past_start.size() << " / " << sanctionedminsday << "  \r";
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
		run_warmups(elf, *tasksit, all_toys);
		auto t = all_toys.get_first_arrival(tasksit->longtask.len);
		elf.do_task(t);
		++tasksdone;
		//if ( tasksdone % 1000 == 0 ) { cout << tasksdone << "/" << tasks.size() << "   \r"; }
	}
}

/** Run the elves over the packaged tasks, naively
 */
void do_packaged_tasks_naive(All_Elves& our_elves, std::vector<AssembledTasks>& tasks, Raw_Toys& all_toys)
{
	for (const auto& task : tasks) {
		auto& elf = our_elves.next_available_elf();

		for (const auto& t : task.warmups) {
			auto tt = all_toys.get_first_arrival(t.len);
			elf.move_to_next_san_hour();
			if ( elf.max_task_in_san_hours() < t.len ) {
				elf.skip_to_start_day();
			}
			elf.do_task(tt);
		}
		auto tt = all_toys.get_first_arrival(task.longtask.len);
		elf.do_task(tt);
	}
}

/** Just removes task from all_toys */
void do_packaged_tasks_simulate(All_Elves& our_elves, std::vector<AssembledTasks>& tasks, Raw_Toys& all_toys)
{
	for (const auto& at : tasks) {
		auto t = all_toys.get_first_arrival(at.longtask.len);
		for (const auto& x : at.warmups) {
			t = all_toys.get_first_arrival(x.len);
		}
	}
}

// ============================================================================

/** Deal with the "other" tasks, those which are of medium size.
 *  Gets the elf to do what it can in sanctioned time, building productivity
 *  and when we hit 4.0, do the long task which we've buffered up for that elf.
 *  Once we run out of tasks we can do, just do the long task anyway.
 *  The `what_is_long` parameter says to ignore tasks longer than this until the end
 *    (this gives a modest improvement).
 *  This takes no account of arrival time!!
 */
int do_others_old(task_buffer* const tasks, All_Elves &our_elves, const int what_is_long = 100000)
{
	// Basic algorithm
	while ( tasks->remaining() > 0 ) {
		auto& e = our_elves.next_available_elf();
		if ( e.stored.num_stored() == 0 ) {
			// Get the longest
			int maxtask = tasks->find_closest_length_leq( what_is_long );
			if ( maxtask <= 0 ) {
				maxtask = tasks->find_largest_length();
			}
			e.stored.put( tasks->extract_task( maxtask ) );
			if ( tasks->remaining() == 0 ) { break; }
		}
		// If at 4.0 do long task
		if ( e.get_prod() >= 4.0 ) {
			// Quite important to time this correctly
			int elf_time = e.raw_time_for_task( e.stored.peek().len );
			int san1, unsan1;
		   std::tie(san1,unsan1) = split_sanctioned_unsanctioned(e.next_available(), elf_time);
			// Compare to moving to next day
			int local_time = e.next_available() % minsday;
			int nextday = e.next_available();
			if ( local_time <= daystarts ) {
				nextday += daystarts - local_time;
			} else {
				nextday += daystarts + minsday - local_time;
			}
			int san2, unsan2;
		   std::tie(san2,unsan2) = split_sanctioned_unsanctioned(nextday, elf_time);
		   if ( unsan2 < unsan1 ) { // Okay, so advantage to skipping to next day
				e.skip_to_start_day();
			}
			e.do_task( e.stored.get() );		
		} else {
			// Do the longest task we can in current day
			int maxtask = e.max_task_if_in_san_hours();
			maxtask = tasks->find_closest_length_leq( maxtask );
			if ( maxtask <= 0 ) {
				// If we skipped to start of next day, would that help?
				maxtask = tasks->find_closest_length_leq( e.max_task_at_cur_prod(sanctionedminsday) );
				if ( maxtask <= 0 ) {
					// Nope, so just do the long task
					e.do_task( e.stored.get() );
				} else {
					e.skip_to_start_day();
					e.do_task( tasks->extract_task( maxtask ) );
				}
			} else {
				e.move_to_next_san_hour();
				e.do_task( tasks->extract_task( maxtask ) );
			}
		}
	}
	// So no tasks left, but elfs may still have local stores
	while ( our_elves.active_elves() > 0 ) {
		auto& e = our_elves.next_available_elf();
		if ( e.stored.num_stored() > 0 ) {
			e.do_task(e.stored.get());
		}
		if ( e.stored.num_stored() ==  0 ) {
			our_elves.finished(e);
		}
	}
	return our_elves.maximum_finish_time();
}

/** Helper function: Returns the maximum task the elf can do, skips to next day if that
 *  will help with finding a task, returns -1 if no tasks.
 */
int prep_elf_max_task(Elf_Work& e, const task_buffer* const tasks)
{
	int maxtask = e.max_task_if_in_san_hours();
	maxtask = tasks->find_closest_length_leq( maxtask );
	if ( maxtask <= 0 ) {
		// If we skipped to start of next day, would that help?
		maxtask = tasks->find_closest_length_leq( e.max_task_at_cur_prod(sanctionedminsday) );
		if ( maxtask <= 0 ) {
			// So basically run out, break and let next part deal!
			return -1;
		} else {
			e.skip_to_start_day();
		}
	} else {
		e.move_to_next_san_hour();
	}
	return maxtask;
}

/** Return a pair: (length, arrival)
 *  If arrival == time then length is the longest task <= `maxlen` which is available at or before `time`.
 *  Otherwise returns the first available task of length <= `maxlen`, or -1 if none exists.
 */
std::pair<int,int> longest_task_available(const task_buffer& tasks, Raw_Toys& all_toys, const int time, const int maxlen)
{
	int newmaxlen = maxlen;
	int best_available = -1;
	int best_available_len = 0;
	while ( newmaxlen > 0 ) {
		newmaxlen = tasks.find_closest_length_leq(newmaxlen);
		if ( newmaxlen > 0 ) {
			int arrival = all_toys.peek_first_arrival(newmaxlen);
			if ( arrival <= time ) {
				return std::pair<int,int>(newmaxlen, time);
			}
			if ( best_available == -1 or arrival < best_available ) {
				best_available = arrival; best_available_len = newmaxlen;
			}
		}
		--newmaxlen;
	}
	return std::pair<int,int>(best_available_len, best_available);
}

/** Deal with the "other" tasks, those which are of medium size.
 *  Gets the elf to do what it can in sanctioned time, building productivity
 *  and when we hit 4.0, do the long task which we've buffered up for that elf.
 *  Once we run out of tasks we can do, just do the long task anyway.
 *  The `what_is_long` parameter says to ignore tasks longer than this until the end
 *    (this gives a modest improvement).
 *  This version correctly uses `all_toys` to only do tasks once they are available.
 *  The algorithm here is that if an elf wishes to do a task not currently available, then
 *  we artificially push it's time to when that task would be available, and do it.
 *
 *  1st algorithm, finishing with old, gives           11605039
 *  2nd ------------------------------------ (4500)     7018809
 *  Starting the old algorithm at end of year (4500)    7046088
 */
void do_others(task_buffer* const tasks, All_Elves &our_elves, Raw_Toys& all_toys, const int what_is_long = 100000)
{
	// Basic algorithm
	while ( tasks->remaining() > 0 ) {
		auto& e = our_elves.next_available_elf();
		// If at 4.0 do long task
		if ( e.get_prod() >= 4.0 ) {
			// Get the longest / first available
			//auto maxtask = longest_task_available(*tasks, all_toys, e.next_available(), what_is_long);
			//if ( maxtask.first == -1 ) { return; } // Our of tasks <= what_is_long
			//if ( maxtask.second > e.next_available() ) {
			//	e.set_available_time( maxtask.second );
			//	e.move_to_next_san_hour(); // The following code will skip to next day if sensible
			//}
			//int maxlen = maxtask.first;
			// Alternative algorithm: get the longest and skip to when available!
			int maxlen = tasks-> find_closest_length_leq( what_is_long );
			if ( maxlen == -1 ) { return; }
			int arrival = all_toys.peek_first_arrival(maxlen);
			if ( e.next_available() < arrival ) {
				e.set_available_time(arrival); e.move_to_next_san_hour();
			}
			// Quite important to time this correctly
			int elf_time = e.raw_time_for_task( maxlen );
			int san1, unsan1;
		   std::tie(san1,unsan1) = split_sanctioned_unsanctioned(e.next_available(), elf_time);
			// Compare to moving to next day
			int local_time = e.next_available() % minsday;
			int nextday = e.next_available();
			if ( local_time <= daystarts ) {
				nextday += daystarts - local_time;
			} else {
				nextday += daystarts + minsday - local_time;
			}
			int san2, unsan2;
		   std::tie(san2,unsan2) = split_sanctioned_unsanctioned(nextday, elf_time);
		   if ( unsan2 < unsan1 ) { // Okay, so advantage to skipping to next day
				e.skip_to_start_day();
			}
			//cout << "At 4.0, doing task of length " << maxlen << "  / " << tasks->remaining() << "\n";
			auto t = tasks->extract_task(maxlen);
			t = all_toys.get_first_arrival( t.len );
			e.do_task(t);
		} else {
			// Do the longest task we can in current day, taking account of arrival times
			int maxtask = prep_elf_max_task(e,tasks);
			if ( maxtask == -1 ) { return; }
			// Now see if we can _actually_ do such a task.
			int arrival = all_toys.peek_first_arrival(maxtask);
			if ( arrival == -1 ) { throw std::runtime_error("do_others:: bugger2."); }
			if ( e.next_available() < arrival ) {
				// Can't, so instead find best task we can do.
				auto pair = longest_task_available(*tasks, all_toys, e.next_available(), maxtask);
				if ( pair.first <= 0 ) {
					throw std::runtime_error("take_account_arrival_time:: Shouldn't get here!");
				}
				maxtask = pair.first;
				e.set_available_time(pair.second); e.move_to_next_san_hour();
				if ( e.max_task_in_san_hours() < maxtask ) { e.skip_to_start_day(); }
			}
			//cout << "Building... doing task of length " << maxtask << "  / " << tasks->remaining() << "\n";
			tasks->extract_task( maxtask );
			auto t = all_toys.get_first_arrival( maxtask );
			e.do_task(t);
		}
		//if ( tasks->remaining() % 1000 == 0 ) { cout << tasks->remaining() << ", " << e.next_available() <<"    \r"; }
	}
}


// ============================================================================

/** Returns the length of the first available task which is in tasks */
int first_available(Raw_Toys& all_toys, task_buffer_map& tasks)
{
	int len = tasks.find_largest_length();
	int bestlen = len;
	int besttime = all_toys.peek_first_arrival(len);
	if ( besttime == -1 ) { throw std::runtime_error("first_available problem"); }
	while ( len > 1 ) {
		--len;
		len = tasks.find_closest_length_leq(len);
		if ( len <= 0 ) { break; } // No more tasks
		int time = all_toys.peek_first_arrival(len);
		if ( time == -1 ) {
			throw std::runtime_error("first_available problem 2");
		}
		if ( time < besttime ) { besttime = time; bestlen = len; }
	}
	return bestlen;
}

/** What to do: wait for long task to arrive, or do a task from `tasks`, killing
 * productivity.  Recall that 500,000 is about the end of the year
 * E.g. 47473 arrives at 157446.  Doing at 0.25 productivity takes an excess 237 days.
 *     Waiting wastes around 110 days.
 * For 25239 arrives at 303673.  Doing at 0.25 productivity takes an excess 126 days.
       Waiting wastes around 210 days.
 */
void assign_long_tasks_from_start(All_Elves& our_elves, Raw_Toys& all_toys, task_buffer_map& tasks)
{
	for (int n = 0; n < 900; ++n) {
		auto t = all_toys.get_first_arrival( all_toys.longest_task() );
		int excessmins = t.len * 3;
		int wastedsanmins = sanctionedminsday * (t.arrival_time / minsday);
		if ( excessmins <= wastedsanmins ) {
			// Doing tasks before is better...
			// We'll pick these from tasks; it doesn't matter what we do.
			auto& e = our_elves.next_available_elf();
			do {
				int len = first_available(all_toys, tasks);
				auto t = tasks.extract_task(len);
				t = all_toys.get_first_arrival(t.len);
				e.move_to_next_san_hour();
				e.do_task(t);
				e.move_to_next_san_hour();
			} while ( e.next_available() < t.arrival_time );
			e.do_task(t);
		} else {
			// So just skip elf to time
			auto& e = our_elves.next_available_elf();
			e.set_available_time(t.arrival_time);
			e.move_to_next_san_hour();
			e.do_task(t);
		}
	}
	// Now loop through elves until all past magic number of 500000 minutes
	while ( true ) {
		auto& e = our_elves.next_available_elf();
		if ( e.next_available() >= 500000 ) { break; }
		int len = first_available(all_toys, tasks);
		auto t = tasks.extract_task(len);
		t = all_toys.get_first_arrival(t.len);
		e.move_to_next_san_hour();
		e.do_task(t);
	};
}

int main()
{
	// Load all toys
	Raw_Toys all_toys = load_raw_toys("../toys_rev2.npy");
	cout << "Total number of toys: " << all_toys.size() << "\n";

	// Load output of optimiser2
	task_buffer_map tasks;
	std::ifstream tasksin("tasks2.save", std::ios::binary);
	tasks.load(tasksin);
	cout << "Loaded other tasks, number: " << tasks.remaining();
	cout << "   range: " << tasks.find_closest_length_geq(1) << " -- ";
	cout << tasks.find_largest_length() << "\n";

	std::vector<AssembledTasks> packaged = load_opt2_output("opt2.npy");
	cout << "Packaged tasks: packages: " << packaged.size();
	{
		int count = 0;
		for (const auto& p : packaged) { count += 1;
			count += p.warmups.size();
		}
		cout << " total tasks: " << count << "\n";
	}
	
	// Idea is to run the algorithm from optimiser3, but taking account of when the tasks actually arrive...

	Logger ourlog;
	All_Elves our_elves(900, &ourlog);
	// Something very special about 4500;  get 6,984,084 mins finish time
	//for (auto& e : our_elves.get_list_elves()) { e.set_available_time(495900); }
	//do_others_old(&tasks, our_elves, 4500);
	// Alternative new algorithm, get 6,913,604 mins finish time, again magic number matters!
	do_others(&tasks, our_elves, all_toys, 4500);
	cout << "do_others finished... and remaining tasks are: " << tasks.remaining();
	if ( tasks.remaining() > 0 ) {
		cout << "   range: " << tasks.find_closest_length_geq(1) << " -- ";
		cout << tasks.find_largest_length() << "\n";
	} else { cout << "\n"; }
	
	// Now, in one version, we've done a lot of work, and expect to be at 4.0 productivity.
	// In the other version, we've done no work!
	auto& e = our_elves.next_available_elf();
	if ( e.next_available() == 540 ) {
		cout << "Running very long tasks and moving elves to later times...\n";
		assign_long_tasks_from_start(our_elves, all_toys, tasks);
	} else {
		cout << "We assume all elves now at max productivity; verifying...\n";
		for (auto& e : our_elves.get_list_elves()) {
			if ( e.get_prod() < 4.0 ) { return 1; }
		}
		cout << "So doing longest tasks...\n";
		// We assume that in optimiser2 we removed the 900 longest tasks, so should be
		// left in tasks
		for (int n = 0; n < 900; ++n) {
			auto& e = our_elves.next_available_elf();
			//if ( e.get_prod() != 4.0 ) { cout << "Problem...\n"; return 2; }
			auto t = all_toys.get_first_arrival( all_toys.longest_task() );
			e.do_task(t);
		}
	}
	cout << "Now assigning the packaged tasks.\n";
	do_packaged_tasks(our_elves, packaged, all_toys);
	//do_packaged_tasks_naive(our_elves, packaged, all_toys);
	//do_packaged_tasks_simulate(our_elves, packaged, all_toys);
	// Do remainder
	cout << "Just have a few tasks left: count = " << all_toys.size() << "\n";

	//auto rem = all_toys.get_list_lens();
	//std::vector<int> shape;
	//shape.push_back(rem.size());
	//save_npy("opt4_left.npy", shape, "i4", rem.data());
	while ( all_toys.size() > 0 ) {
		auto& e = our_elves.next_available_elf();
		auto t = all_toys.get_first_arrival( all_toys.longest_task() );
		e.do_task(t);
	}
	
	// Save logger output
	ourlog.save_as_npy("optimiser4.npy");
	cout << "Finish time: " << our_elves.maximum_finish_time() << "\n";

	return 0;
}
