/** elf_work.cpp
 *
 *  Implements class Elf_Work which supports further functionality to the Elf class,
 *  specifically for assigning tasks and so forth.
 *
 *  Also includes a simple logging output, if required.
 */

#include "elf_work.h"
using namespace Elfns;
#include <cmath>

// ======================================================================================
// Logger

#include "save_npy.h"

void Logger::add_entry(const int elfid, const int tasklen, const int time, const int elfduration)
{
	log.push_back( entry{ elfid, tasklen, time, elfduration } );
}

void Logger::save_as_npy(std::string filename)
{
	std::vector<int> shape;
	shape.push_back(log.size());
	shape.push_back(4);
	save_npy(filename, shape, "i4", log.data());
}



// ======================================================================================
// Elf_Work_Base
//
// Added in later, to factor out some basic functionality we need.

void Elf_Work_Base::move_to_next_san_hour()
{
	int localmins = available_to_work % minsday;
	if ( localmins < daystarts ) {
		available_to_work += daystarts - localmins;
	} else {
		if ( localmins >= dayends ) {
			available_to_work += minsday - localmins + daystarts;
		}
	}
}

void Elf_Work_Base::skip_to_start_day()
{
	move_to_next_san_hour(); // Make sure in sanctioned hours
	int localmins = available_to_work % minsday;
	if ( localmins == daystarts ) { return; }
	available_to_work += minsday - localmins + daystarts;
}

/** Return the longest task the elf can perform in time `len`.
 */
int Elf_Work_Base::max_task_at_cur_prod(const int len)const
{
	return floor( productivity * len );
}

/** Return the longest task the elf can perform in sanctioned hours.
 *  Assumes elf has already been pushed to sanctioned hours.
 */
int Elf_Work_Base::max_task_in_san_hours()const
{
	int localmins = available_to_work % minsday;
	return max_task_at_cur_prod(dayends - localmins);
}

int Elf_Work_Base::do_task(const int len)
{
	return Elf::do_task(available_to_work, len);
}

int Elf_Work_Base::do_task(toy_task t)
{
	return Elf::do_task(available_to_work, t.len);
}

void Elf_Work_Base::set_prod(const double p)
{
	if ( p <= 0.25 ) { productivity = 0.25; return; }
	if ( p >= 4.0 ) { productivity = 4.0; return; }
	productivity = p;
}

int Elf_Work_Base::raw_time_for_task(const int len)const
{
	return ceil(len / productivity);
}



// ======================================================================================
// Elf_Work

// Returns the maximum task we could do, assuming we have moved to the next
// sanctioned hour.  But doesn't change anything.
int Elf_Work::max_task_if_in_san_hours()
{
	int backup_avail = available_to_work;
	move_to_next_san_hour();
	int r = max_task_in_san_hours();
	available_to_work = backup_avail;
	return r;
}

int Elf_Work::do_task(const int len)
{
	if ( logptr != nullptr ) {
		logptr->add_entry(id, len, available_to_work, static_cast<int>(ceil(len/productivity)));
	}
	last_finish_time = available_to_work + raw_time_for_task(len);
	return Elf_Work_Base::do_task(len);
}

int Elf_Work::do_task(toy_task t)
{
	return do_task(t.len);
}

int Elf_Work::task_length_to_attain_prod(const double target_prod)const
{
	// Solve e.productivity * (1.02)**t = target_prod
	if ( productivity >= target_prod ) { return 0; }
	double t = ( log(target_prod) - log(productivity) ) / log(1.02);
	// Now need 60*t <= ceil(task * productivity)
	int task = floor(60*t / productivity);
	while ( ceil(task * productivity) < 60*t ) { ++task; }
	return task;
}

