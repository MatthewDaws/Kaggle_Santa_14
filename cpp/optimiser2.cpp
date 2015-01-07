/** optimiser2.cpp
 *
 *  Turn the problem around and assign the "short" tasks (150 - 130 minutes, say) to the longer
 *  tasks which need them most.
 *
 *  For this, we assume our elves will start at 9am each time, with 0.25 productivity.
 *     - Maybe not true for the longest 900 tasks, which we might hope to attack at the start
 *  Then we loop through all the short tasks (150 -- 130, or whatever) and assign to the long task
 *  for which we'll see the greatest decrease in time.
 *  If we end up trying to assign two such short tasks, then we search for a longer task (Elf gets up
 *  to about 0.305, so can do 182 minute task) instead.
 *  Then save out, and look at the data in IPython
 *
 *  Ultimately, we want to get the Elf's doing these tasks, assigning so that the 'wasted' time,
 *  waiting for the start of the next day, is minimised.  Note that as the assumption is that the
 *  productivity will always be reduced to 0.25, we could intermix these with the tasks we can't
 *  do anything about.
 *
 *  Alternatively, run the same algorithm assuming a higher starting productivity (say 1.0) on
 *  the remaining tasks.  Not sure how to enforce the constraint that the productivity cannot fall
 *  too far.  Maybe run and rerun, removing the too long tasks?
 */
 
#include "elf_work.h"
using namespace Elfns;
#include "tasks.h"
#include "save_npy.h"

#include <iostream>
using std::cout;
using std::endl;
#include <cmath>
#include <algorithm>
#include <fstream>

/** Helper function: move elf to next sanctioned hour, and try to run task.
 *  If can't, nove to next day and try again.
 *  If still can't, return false, else true.
 */
bool elf_do_work_procedure(Elf_Work_Base &elf, const int len)
{
	if ( elf.max_task_at_cur_prod(sanctionedminsday) < len ) { return false; }
	elf.move_to_next_san_hour();
	if ( elf.max_task_in_san_hours() < len ) {
		elf.skip_to_start_day();
	}
	elf.Elf::do_task(elf.next_available(), len); // Manually do task
	return true;
}



/** Stores list of tasks, and an Elf which executes them.
 *  Convention is that tasks[0] is the long task, and tasks[1..] are shorter tasks to get
 *  productivity up.
 *  `elf` should always be at the point immediately before attacking the long task.
 *  so initially at 9am with productivity = 0.25
 */
class Long_Task {
public:
	toy_task longtask;
	std::vector<toy_task> tasks;
	int benefit;
	Elf_Work_Base elf;

	Long_Task(toy_task t) : longtask(t), benefit(0) { elf.set_prod(0.25); }
	int trial_extra_task(const int len)const;
	bool operator<(const Long_Task& rhs)const;
};

/** Ordering so we can use a heap.
 *  Break ties by length of long task
 */
bool Long_Task::operator<(const Long_Task& rhs)const
{
	if ( this->benefit == rhs.benefit ) {
		return this->longtask.len < rhs.longtask.len;
	}
	return this->benefit < rhs.benefit;
}

/** Returns the time saved (or negative, for extra time needed) by doing another
 *  task of length `len` before the long task.
 *  Returns -1 if len cannot be run (due to productivity)
 */
int Long_Task::trial_extra_task(const int len)const
{
	if ( elf.max_task_at_cur_prod(sanctionedminsday) < len ) { return -1; }
	Elf_Work_Base elfcopy = elf;
	elf_do_work_procedure(elfcopy, len);
	int newtime = elfcopy.next_available() + elfcopy.raw_time_for_task(longtask.len);
	int oldtime = elf.next_available() + elf.raw_time_for_task(longtask.len);
	return oldtime - newtime;
}


/** Allocates tasks from shortlow (5?) to shorthigh (150) according to which long task will
 *  benefit most.
 */
template <typename TaskType>
void algorithm1(TaskType& tasks, std::vector<Long_Task>& long_tasks)
{
	constexpr int shortlow = 1, shorthigh = 150;
	int oldlen = -1; // For caching
	while ( true ) {
		int len = tasks.find_closest_length_leq(shorthigh);
		if ( len < shortlow ) { break; } // Run out
		// Find long task which will most benefit
		if ( len != oldlen ) { // Rebuild list
			cout << "Rebuilding for size " << len << "\n";
			for (auto& lt : long_tasks) {
				lt.benefit = lt.trial_extra_task(len);
			}
			std::make_heap(long_tasks.begin(), long_tasks.end());
			oldlen = len;
		}
		auto maxit = long_tasks.end() - 1;
		std::pop_heap(long_tasks.begin(), long_tasks.end());
		toy_task t;
		if ( maxit->tasks.size() > 0 ) { // Could do a longer task?
			auto elf = maxit->elf;
			// Old algorithm: only skips to next day if smaller length doesn't fit.
			//elf.move_to_next_san_hour();
			//if ( elf.max_task_in_san_hours() < len ) { elf.skip_to_start_day(); }
			//int maxlen = elf.max_task_in_san_hours();
			// New algorithm: get longest possible task
			int maxlen = elf.max_task_at_cur_prod(sanctionedminsday);
			t = tasks.extract_task( tasks.find_closest_length_leq(maxlen) );
		} else {
			t = tasks.extract_task(len);
		}
		maxit->tasks.push_back(t);
		elf_do_work_procedure(maxit->elf, t.len);
		maxit->benefit = maxit->trial_extra_task(len);
		//cout << " new prod: " << maxit->elf.get_prod() << " new benefit: " << maxit->benefit << "\n";
		std::push_heap(long_tasks.begin(), long_tasks.end());
	}
}

/** Takes the output from algorithm1, takes out the longer (>150) tasks which we can without
 *  reordering.  Then assigns them using the same principle as before: put a task to work on
 *  the long task which'll benefit most (and which the elf can actually do).
 *
 *  TODO: also allow reordering??
 */
template <typename TaskType>
void algorithm2(TaskType& tasks, std::vector<Long_Task>& long_tasks)
{
	// Strip out >150 tasks
	/*for (auto& lt : long_tasks) {
		while ( lt.tasks.back().len > 150 and lt.tasks.size() > 1 ) {
			// Have to add back into the task handler.
			// Nasty hack...
			tasks.add_task( toy_task{0, 0, lt.tasks.back().len} );
			// Remove from long_tasks list
			lt.tasks.pop_back();
		}
	}
	tasks.set_for_use();*/
	// Have to decide on the most sensible way to allocate: probably do want to allow reordering, to
	// avoid lots of "skipping to the next day".
	// For the moment, don't do this, to allow to copy the other algorithm
	int oldlen = -1; // For caching
	int len = 151; // Current place to loop
	while ( true ) {
		len = tasks.find_closest_length_geq(len);
		if ( len < 0 or len > 600*4 ) { return; } // Run out
		// Find long task which will most benefit
		if ( len != oldlen ) { // Rebuild list
			cout << "Rebuilding for size " << len;
			int count = 0;
			for (auto& lt : long_tasks) {
				lt.benefit = lt.trial_extra_task(len);
				if ( lt.benefit > 0 ) { ++count; }
			}
			std::make_heap(long_tasks.begin(), long_tasks.end());
			oldlen = len;
			cout << " possibilities: " << count << "\n";
		}
		// Pop out anyway; if we can't use it, we can't use it...
		if ( long_tasks[0].benefit > 0 ) { // This is currently the "top" of the heap
			//cout << "Going to use for task length: " << long_tasks[0].longtask.len << "\n";
			auto maxit = long_tasks.end() - 1;
			std::pop_heap(long_tasks.begin(), long_tasks.end());
			toy_task t = tasks.extract_task(len);
			maxit->tasks.push_back(t);
			elf_do_work_procedure(maxit->elf, t.len);
			maxit->benefit = maxit->trial_extra_task(len);
			std::push_heap(long_tasks.begin(), long_tasks.end());
		} else { // Nothing can benefit at this len, so advance
			++len;
		}
	}
}


int main()
{
	// Load up a list of Long_Task, for all tasks over 4000 minutes, say
	cout << "Building list of longer tasks.\n";
	constexpr int minlong = 4000;
	std::vector<Long_Task> long_tasks;
	{
		auto alltoys = load_toys_vector("../toys_rev2.npy");
		for (const auto& t : alltoys) {
			if ( t.len >= minlong ) {
				long_tasks.push_back( Long_Task{t} );
			}
		}
	}
	
	// Now load into a tasks handler
	cout << "Loading tasks.\n";
	auto tasks = load_toys_map("../toys_rev2.npy");

	// In new version, discard the 900 longest tasks, to be done as a special case
	std::sort(long_tasks.begin(), long_tasks.end());
	for (int n = 0; n < 900; ++n) {
		long_tasks.pop_back();
		tasks.extract_task( tasks.find_largest_length() );
	}

	// Run the algorithm
	cout << "Running algorithm.\n";
	algorithm1(tasks, long_tasks);
	//algorithm2(tasks, long_tasks);

	cout << "Done...\n";
	// Save
	// Firstly in our format.  For this, we need to `extract` the longer tasks we can deal with
	for (const auto& lt : long_tasks) {
		if ( lt.tasks.size() > 0 ) {
			tasks.extract_task(lt.longtask.len);
		}
	}
	std::ofstream tasksout("tasks2.save", std::ios::binary);
	tasks.save(tasksout);
	tasksout.close();
	// Format will be a list of tasks, assumed small tasks, then big tasks.
	cout << "Saving in npy format...\n";
	struct save_format {
		int toyid, toylen;
	};
	std::vector<save_format> tosave;
	std::sort(long_tasks.begin(), long_tasks.end(), [](const Long_Task& lhs, const Long_Task& rhs) {
		return lhs.longtask.len > rhs.longtask.len; });
	for (const auto& lt : long_tasks) {
		if ( lt.tasks.size() > 0 ) {
			for (const auto& t : lt.tasks) {
				tosave.push_back( save_format{ t.id, t.len } );
			}
			tosave.push_back( save_format{ lt.longtask.id, lt.longtask.len } );
		}
	}
	save_npy("opt2.npy", std::vector<int>{static_cast<int>(tosave.size()),2}, "i4", tosave.data());

	return 0;
}









// ============================================================================
// Not currently used

/** Finds the longest `len` such that can fit `len` and then `longer` into a day,
 *  assuming a start productivity of 0.25
 */
int max_start_task(const int longer)
{
	int guess = 150 - longer;
	int actual_end;
	do {
		++guess;
		Elf_Work_Base e;
		e.set_prod(0.25);
		e.Elf::do_task(e.next_available(), guess);
		e.Elf::do_task(e.next_available(), longer);
		actual_end = e.next_available();
	} while ( actual_end < dayends );
	if ( actual_end > dayends ) { --guess; }
	return guess;
}
