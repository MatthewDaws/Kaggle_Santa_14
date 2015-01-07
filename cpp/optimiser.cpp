/** Optimiser.cpp
 *
 *  Currently a portmanteau file doing all the optimising.
 *
 */

#include "tasks.h"
#include "elf_work.h"
using namespace Elfns;

#include <iostream>
using std::cout;
using std::endl;

#include <vector>
#include <algorithm>
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


// ======================================================================================
#include "tasks.h"

int test_toys_map()
{
	auto atasks = load_toys_map("../toys_rev2.npy");
	task_buffer *tasks = &atasks; // Not really sure why we're doing this
	cout << "Loaded " << tasks->remaining() << " tasks loaded." << endl;
	cout << "Longest task is of length " << tasks->find_largest_length();
	toy_task t = tasks->extract_task( tasks->find_largest_length() );
	cout << " which is id: " << t.id << " arriving at " << t.arrival_time << " len:" << t.len << endl;
	cout << "Extracting 10 tasks of length 1...\n";
	for (int i=0; i<10; ++i) {
		toy_task t = tasks->extract_task( 1 );
		cout << "  toy:: id: " << t.id << " arriving at " << t.arrival_time << " len:" << t.len << endl;
	}
	cout << "Checking longest task again: " << tasks->find_largest_length();
	cout << " should be same: " << tasks->find_closest_length_leq(100000000) << "\n";
	cout << "Will exhaust all tasks of length <= 10...";
	while ( true ) {
		int len = tasks->find_closest_length_leq(10);
		if ( len == -1 ) { break; }
		toy_task t = tasks->extract_task( len );
		if ( t.len != len ) { cout << "Problem!\n"; return 1; }
	}
	cout << " have left " << tasks->remaining() << "\n";
	cout << "Smallest task now is: " << tasks->find_closest_length_geq(10) << "\n";
	return 0;
}


// ======================================================================================


int main()
{
	//return test_toys_map();

	auto atasks = load_toys_map("../toys_rev2.npy");
	task_buffer *tasks = &atasks; // Not really sure why we're doing this

	// Initialise our Elves
	Logger ourlog;
	All_Elves our_elves(900, &ourlog);
	cout << "And we're off..." << endl;
	// Basic algorithm
	while ( tasks->remaining() > 0 ) {
		auto& e = our_elves.next_available_elf();
		if ( e.stored.num_stored() == 0 ) {  // Do the work to assign new tasks
			// Get the longest
			e.stored.put( tasks->extract_task( tasks->find_largest_length() ) );
			if ( tasks->remaining() == 0 ) { break; }
			// Time left in current day?
			// If at start of day, then do what we can, then long task...
			int time_into_san_hours = e.next_available() % minsday;
			if ( time_into_san_hours < daystarts or time_into_san_hours >= dayends ) {
				time_into_san_hours = 0;
			} else {
				time_into_san_hours -= daystarts;
			}
			if ( time_into_san_hours <= 30 ) { // MAGIC number!
				// Just do single task to get productivity up.
				int maxtask = e.max_task_if_in_san_hours();
				maxtask = tasks->find_closest_length_leq( maxtask );
				if ( maxtask <= 0 ) {
					// Nothing which'll fit, so for now do nothing.
				} else {
					e.move_to_next_san_hour();
					e.do_task( tasks->extract_task( maxtask ) );
				}
				// Sneak in small task?
				maxtask = tasks->find_closest_length_leq( e.max_task_in_san_hours() );
				if ( maxtask > 0 ) {
					e.move_to_next_san_hour();
					e.do_task( tasks->extract_task( maxtask ) );
				}
			} else {
				// So do a short task in current day, _and_ a longer task next day, if possible
				int maxtask = e.max_task_if_in_san_hours();
				maxtask = tasks->find_closest_length_leq( maxtask );
				if ( maxtask <= 0 ) {
					// Nothing which'll fit.
				} else {
					e.move_to_next_san_hour();
					e.do_task( tasks->extract_task( maxtask ) );
					if ( tasks->remaining() == 0 ) { break; }
				}
				// Sneak in small task?
				maxtask = tasks->find_closest_length_leq( e.max_task_in_san_hours() );
				if ( maxtask > 0 ) {
					e.move_to_next_san_hour();
					e.do_task( tasks->extract_task( maxtask ) );
				}
				// Now close to end of day; check if we can find a task to do entirely in the next day?
				maxtask = tasks->find_closest_length_leq( e.max_task_at_cur_prod(sanctionedminsday) );
				if ( maxtask <= 0 ) {
					// Just leave long task then
				} else {
					// *Problem* if we've run out of tasks around 150 in size
					if ( e.max_task_in_san_hours() < maxtask ) {
						e.skip_to_start_day();
					}
					e.do_task( tasks->extract_task( maxtask ) );
				}
			}
		}
		// Do the work!
		e.do_task( e.stored.get() );
	}
	// So no tasks left, but elfs may still have local stores
	cout << "Ending outstanding elf tasks..." << endl;
	while ( our_elves.active_elves() > 0 ) {
		auto& e = our_elves.next_available_elf();
		if ( e.stored.num_stored() > 0 ) {
			e.do_task(e.stored.get());
		}
		if ( e.stored.num_stored() ==  0 ) {
			our_elves.finished(e);
		}
	}
	// Save logger output
	cout << "Saving output..." << endl;
	ourlog.save_as_npy("optimiser.npy");
	cout << "Done..." << endl;
	return 0;
}
