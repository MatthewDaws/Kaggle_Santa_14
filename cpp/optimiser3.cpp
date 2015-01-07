/** optimiser3.cpp
 *
 *  Deal with the output of optimiser2.cpp; namely the remaining tasks...
 */


#include "elf_work.h"
using namespace Elfns;
#include "tasks.h"

#include <iostream>
using std::cout;
using std::endl;
#include <cmath>
#include <algorithm>
#include <fstream>
#include <vector>
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

// If we aim for lower than 4.0 then need to make sure we end up at a high enough prod
// to have work to do.  But this _could_ be better, as it makes the smaller tasks last
// longer.  Isn't better!
void algorithm1(task_buffer* const tasks, const int what_is_long = 100000)
{
	// Initialise our Elves
	Logger ourlog;
	All_Elves our_elves(900, &ourlog);
	cout << "And we're off..." << endl;
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
		// Can we do the long task and maintain a productivity of >= 0.39 (234 min task in day)?
		// Skip to next day, for best possible result
		int local_time = e.next_available() % minsday;
		int nextday = e.next_available();
		if ( local_time <= daystarts ) {
			nextday += daystarts - local_time;
		} else {
			nextday += daystarts + minsday - local_time;
		}
		int san, unsan;
	   std::tie(san,unsan) = split_sanctioned_unsanctioned(nextday,
	   		e.raw_time_for_task( e.stored.peek().len ));
		double newprod = e.get_prod() * pow(1.02, static_cast<double>(san)/60.0)
				* pow(0.9, static_cast<double>(unsan)/60.0);
		if ( newprod >= 0.39 ) {
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
				//cout << "Can't fit in current day; prod:" << e.get_prod() << "\n";
				//cout << "But could do: " << e.max_task_at_cur_prod(sanctionedminsday) << "\n";
				maxtask = tasks->find_closest_length_leq( e.max_task_at_cur_prod(sanctionedminsday) );
				//cout << "maxtask: " << maxtask << "\n";
				if ( maxtask <= 0 ) {
					// Nope, so just do the long task
					e.do_task( e.stored.get() );
				} else {
					e.skip_to_start_day();
					//cout << "So skipped to start of next day, maxtask: " << maxtask << "\n";
					//cout << "elf next available: " << e.next_available() << "\n";
					//cout << "elf prod: " << e.get_prod() << "\n";
					e.do_task( tasks->extract_task( maxtask ) );
					//cout << "elf prod: " << e.get_prod() << "\n";
				}
			} else {
				e.move_to_next_san_hour();
				e.do_task( tasks->extract_task( maxtask ) );
			}
		}
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
	ourlog.save_as_npy("optimiser3.npy");
	cout << "Done..." << endl;
}


int algorithm(task_buffer* const tasks, const int what_is_long = 100000)
{
	// Initialise our Elves
	Logger ourlog;
	All_Elves our_elves(900, &ourlog);
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
	// Save logger output
	//ourlog.save_as_npy("optimiser3.npy");
	// Return last finish time
	return our_elves.maximum_finish_time();
}



int main()
{
	task_buffer_map tasks;
	std::ifstream tasksin("tasks2.save", std::ios::binary);
	tasks.load(tasksin);

	cout << "Loaded tasks...\n";
	cout << "Smallest task is: " << tasks.find_closest_length_geq(1) << "\n";
	cout << "Largest task is: " << tasks.find_largest_length() << "\n";

	int last_finish = algorithm(&tasks, 8590);
	cout << "Last elf finished at " << last_finish << "\n";

	return 0;
}
