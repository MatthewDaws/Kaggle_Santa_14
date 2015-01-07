/** optimiser5.cpp
 *
 *  Sanity check
 */

#include "tasks.h"
#include "load_npy.h"
#include "save_npy.h"
#include "calendar.h"

#include <iostream>
using std::cout;
using std::endl;
#include <vector>
#include <algorithm>
#include <fstream>

// ============================================================================

/** Load the raw toys */
class Raw_Toys {
public:
	struct toy_data { int starttime, dur; };
	Raw_Toys(std::vector<toy_data> input);
	int peek_first_arrival(const int length)const;
	toy_task get_first_arrival(const int length);
	int size()const { return toys_left; }
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
	--toys_left;
	auto* vec = &toys[length];
	auto t = vec->back();
	vec->pop_back();
	if ( vec->size() == 0 ) {
		vec = nullptr;
		toys.erase(length);
	}
	return toy_task{t.id, t.starttime, length};
}

/** Load in all toys into the Raw_Toys structure */
Raw_Toys load_raw_toys(std::string filename)
{
	auto tmp = load_numpy_structured_rows<Raw_Toys::toy_data>(filename, 2, "i4");
	return Raw_Toys(tmp);
}



int main()
{
	struct opt4out { int elfid, tasktime, starttime, actualtime; };
	auto out = load_numpy_structured_rows<opt4out>("optimiser4.npy", 4, "i4");
	struct toy_data { int starttime, dur; };
	Raw_Toys toys = load_raw_toys("../toys_rev2.npy");

	if ( static_cast<int>(out.size()) != toys.size() ) {
		cout << "Size mismatch!\n"; return 1;
	}

	// Check tasks done at correct time, and assemble output list
	struct output {
		int toyid, elfid, time, actual_duration;
		bool operator<(const output& rhs) { return this->time < rhs.time; }
	};
	std::vector<output> data;
	for (const auto& row : out) {
		output outrow;
		outrow.elfid = row.elfid;
		outrow.time = row.starttime;
		outrow.actual_duration = row.actualtime;
		outrow.toyid = -1;
		int arrival = toys.peek_first_arrival(row.tasktime);
		if ( arrival == -1 ) { cout << "No such length!\n"; return 2; }
		if ( arrival > row.starttime ) {
			cout << "Task length " << row.tasktime << " available at " << arrival << " but scheduled at " << row.starttime << "\n";
			return 3;
		}
		auto t = toys.get_first_arrival(row.tasktime);
		outrow.toyid = t.id;
		data.push_back(outrow);
		//cout << data.size() << "  \r";
	}

	cout << "All checked, saving...\n";
	std::sort(data.begin(), data.end());
	
	// "ToyId,ElfId,StartTime,Duration\n";
	// "1,1,2014 1 1 9 0,5\n";
	std::ofstream file("optimiser5.csv");
	file << "ToyId,ElfId,StartTime,Duration\n";
	for (const auto& row : data) {
		std::chrono::minutes mins(row.time);
		Elf_calendar ec(mins);
		Elf_calendar::tm formatted = ec.time_split();
		file << row.toyid << "," << row.elfid << ",";
		file << formatted.print();
		file << "," << row.actual_duration << "\n";
	};
	std::vector<int> shape;
	shape.push_back( static_cast<int>(data.size()) );
	shape.push_back(4);
	save_npy("optimiser5.npy", shape, "i4", data.data());
}
