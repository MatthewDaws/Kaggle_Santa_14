/** tasks.cpp
 *
 *  Store collections of toys to be made.
 */

#include "tasks.h"
#include "load_npy.h"
#include <algorithm>
#include <iostream>
#include <sstream>

// =====================================================================================
// Sorted list implementation

task_buffer_list::task_buffer_list(const std::vector<toy_task> toys)
{
	data = toys;
	set_for_use();
}

void task_buffer_list::add_task(toy_task t)
{
	data.push_back(t);
}

void task_buffer_list::set_for_use()
{
	std::sort(data.begin(), data.end());
}

/** Find the greatest length which is <= target */
int task_buffer_list::find_closest_length_leq(const int target)const
{
	if ( data[0].len > target ) { return -1; }
	if ( data.back().len <= target ) { return data.back().len; }
	// Binary search
	int index_low = 0, index_high = data.size()-1;
	while ( index_high - index_low >= 2 )
	{
		int index_mid = ( index_high + index_low ) / 2;
		if ( data[index_mid].len <= target ) {
			index_low = index_mid;
		} else {
			index_high = index_mid;
		}
	}
	return data[index_low].len;
}

/** Find the smallest length which is >= target */
int task_buffer_list::find_closest_length_geq(const int target)const
{
	if ( data[0].len >= target ) { return data[0].len; }
	if ( data.back().len < target ) { return -1; }
	// Binary search
	int index_low = 0, index_high = data.size()-1;
	while ( index_high - index_low >= 2 )
	{
		int index_mid = ( index_high + index_low ) / 2;
		if ( data[index_mid].len >= target ) {
			index_high = index_mid;
		} else {
			index_low = index_mid;
		}
	}
	return data[index_high].len;
}

/** Find and remove a task of length len; otherwise throw class failure
 *  Want to find the _first_ entry matching, as this will be the toy which arrived earliest
 */
toy_task task_buffer_list::extract_task(const int length)
{
	if ( data[0].len > length or data.back().len < length )
	{
		throw failure("task_buffer_list::extract_task can't find task of requested length.");
	}
	// Binary search; maintain that data[index_low].len <= length <= data[index_high].len
	int index_low = 0, index_high = data.size()-1;
	while ( index_high - index_low >= 2 )
	{
		int index_mid = ( index_high + index_low ) / 2;
		if ( data[index_mid].len < length ) {
			index_low = index_mid;
		} else {
			if ( data[index_mid].len > length ) {
				index_high = index_mid;
			} else {
				// Equality case: as we're trying to find the _first_ occurance just set index_high
				index_high = index_mid;
			}
		}
	}
	int index;
	if ( data[index_low].len == length ) {
		index = index_low;
	} else {
		if ( data[index_high].len == length ) { index = index_high; }
		else {
			throw failure("task_buffer_list::extract_task can't find task of requested length.");
		}
	}
	auto r = data[index];
	data.erase( data.begin() + index );
	return r;
}



// =====================================================================================
// std::map implementation

task_buffer_map::task_buffer_map(const std::vector<toy_task> toys)
{
	total_count = toys.size();
	for (const auto& t : toys) {
		data[ t.len ].push_back( t );
	}
}

void task_buffer_map::generate_available_lengths()
{
	available_lengths.resize(0);
	for (auto it = data.begin(); it != data.end(); ++it) {
		available_lengths.push_back( it->first );
	}
}

task_buffer_map::task_buffer_map()
{
	total_count = 0;
}

void task_buffer_map::add_task(toy_task t)
{
	data[ t.len ].push_back( t );
	++total_count;
}

#include <functional>
void task_buffer_map::set_for_use()
{
	generate_available_lengths();
	// Sort each one
	for (auto it = data.begin(); it != data.end(); ++it) {
		std::sort(it->second.begin(), it->second.end(), std::greater<toy_task>());
	}
}

int task_buffer_map::find_closest_length_leq(const int target)const
{
	if ( available_lengths[0] > target ) { return -1; }
	if ( available_lengths.back() <= target ) { return available_lengths.back(); }
	// Binary search; maintains al[index_low] <= target < al[index_high]
	int index_low = 0, index_high = available_lengths.size()-1;
	while ( index_high - index_low >= 2 )
	{
		int index_mid = ( index_high + index_low ) / 2;
		if ( available_lengths[index_mid] <= target ) {
			index_low = index_mid;
		} else {
			index_high = index_mid;
		}
	}
	return available_lengths[index_low];
}

int task_buffer_map::find_closest_length_geq(const int target)const
{
	if ( available_lengths[0] >= target ) { return available_lengths[0]; }
	if ( available_lengths.back() < target ) { return -1; }
	// Binary search
	// Invariant: [index_low] < target and [index_high] >= target
	int index_low = 0, index_high = data.size()-1;
	while ( index_high - index_low >= 2 )
	{
		int index_mid = ( index_high + index_low ) / 2;
		if ( available_lengths[index_mid] >= target ) {
			index_high = index_mid;
		} else {
			index_low = index_mid;
		}
	}
	return available_lengths[index_high];
}

int task_buffer_map::find_largest_length()const
{
	return data.rbegin()->second.back().len;
}

toy_task task_buffer_map::extract_task(const int len)
{
	auto it = data.find(len);
	if ( it == data.end() ) {
		std::stringstream ss;
		ss << "task_buffer_map::extract_task failed to find toy of requested len.\n";
		ss << "requested len: " << len << "\n";
		throw failure(ss.str().c_str());
		//throw failure("task_buffer_map::extract_task failed to find toy of requested len.");
	}
	auto t = it->second.back();
	it->second.pop_back();
	total_count--;
	if ( it->second.size() == 0 ) {
		data.erase(it);
		// Binary search of available_lengths
		// Invariant *lowit <= len <= *highit
		auto lowit = available_lengths.begin();
		auto highit = available_lengths.end() - 1;
		while ( highit - lowit >= 2 ) {
			auto midit = lowit + (highit - lowit) / 2;
			if ( *midit <= len ) { lowit = midit; } else { highit = midit; }
		}
		if ( *lowit == len ) {
			available_lengths.erase(lowit);
		} else {
			available_lengths.erase(highit);
		}
	}
	return t;
}

void task_buffer_map::save(std::ostream& out)
{
	// Save a version, giving some minimal control
	int version = 1;
	out.write(reinterpret_cast<char*>(&version), sizeof(version));
	// Write `total_count` variable
	out.write(reinterpret_cast<char*>(&total_count), sizeof(total_count));
	// Write `available_lengths` vector
	write_vector(out, available_lengths);
	// Write `data` std::map
	{
		auto x = data.size();
		out.write(reinterpret_cast<char*>(&x), sizeof(x));
		for (const auto& pair : data) {
			out.write(reinterpret_cast<const char*>(&pair.first), sizeof(pair.first));
			write_vector(out, pair.second);
		}
	}
}

template <typename T>
void task_buffer_map::write_vector(std::ostream& out, const std::vector<T>& data)
{
	auto x = data.size();
	out.write(reinterpret_cast<char*>(&x), sizeof(x));
	for (const auto& x : data) {
		out.write(reinterpret_cast<const char*>(&x), sizeof(x));
	}
}

template <typename T>
std::vector<T> task_buffer_map::read_vector(std::istream& in)
{
	std::vector<T> data;
	auto size = data.size();
	in.read(reinterpret_cast<char*>(&size), sizeof(size));
	data.reserve(size);
	for (decltype(size) n = 0; n < size; ++n) {
		T x;
		in.read(reinterpret_cast<char*>(&x), sizeof(x));
		data.push_back(x);
	}
	return data;
}

void task_buffer_map::load(std::istream& in)
{
	// Check version
	int version = -1;
	in.read(reinterpret_cast<char*>(&version), sizeof(version));
	if ( version != 1 ) { return; }
	// Read in
	int new_total_count;
	in.read(reinterpret_cast<char*>(&new_total_count), sizeof(new_total_count));
	std::vector<int> new_available_lengths = read_vector<int>(in);
	std::map<int,std::vector<toy_task>> new_data;
	auto size = new_data.size();
	in.read(reinterpret_cast<char*>(&size), sizeof(size));
	for (decltype(size) n = 0; n < size; ++n) {
		int key;
		in.read(reinterpret_cast<char*>(&key), sizeof(key));
		new_data[key] = read_vector<toy_task>(in);
	}
	// Copy new data across
	total_count = new_total_count;
	available_lengths = new_available_lengths;
	data = new_data;
}


// =====================================================================================
task_buffer_list load_toys_list(std::string filename)
{
	struct toy_data { int starttime, dur; };
	auto rawtoys = load_numpy_structured_rows<toy_data>(filename, 2, "i4");
	task_buffer_list r;
	for (unsigned int index = 0; index < rawtoys.size(); ++index) {
		r.add_task( toy_task{static_cast<int>(index+1), rawtoys[index].starttime, rawtoys[index].dur} );
	}
	r.set_for_use();
	return r;
}

task_buffer_map load_toys_map(std::string filename)
{
	struct toy_data { int starttime, dur; };
	auto rawtoys = load_numpy_structured_rows<toy_data>(filename, 2, "i4");
	task_buffer_map r;
	for (unsigned int index = 0; index < rawtoys.size(); ++index) {
		r.add_task( toy_task{static_cast<int>(index+1), rawtoys[index].starttime, rawtoys[index].dur} );
	}
	r.set_for_use();
	return r;
}

std::vector<toy_task> load_toys_vector(std::string filename)
{
	struct toy_data { int starttime, dur; };
	auto rawtoys = load_numpy_structured_rows<toy_data>(filename, 2, "i4");
	std::vector<toy_task> tasklist;
	tasklist.reserve(rawtoys.size());
	for (std::vector<toy_data>::size_type index = 0; index < rawtoys.size(); ++index) {
		tasklist.push_back( toy_task{static_cast<int>(index+1), rawtoys[index].starttime, rawtoys[index].dur} );
	}
	return tasklist;
}
