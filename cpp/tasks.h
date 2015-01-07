/** tasks.h
 *
 *  Store collections of toys to be made.
 */

#ifndef __tasks_h
#define __tasks_h

#include <vector>
#include <tuple>
#include <stdexcept>
#include <string>
#include <map>
#include <iosfwd>

/** Store information about the toys. */
struct toy_task {
	int id, arrival_time, len;
	// To support sorting etc.
	bool operator<(const toy_task& rhs)const { return std::tie(len, arrival_time, id) < std::tie(rhs.len, rhs.arrival_time, rhs.id); }
	bool operator>(const toy_task& rhs)const { return std::tie(len, arrival_time, id) > std::tie(rhs.len, rhs.arrival_time, rhs.id); }
};

/** Abstract data type to store list of tasks.
 *
 */
class task_buffer
{
public:
	class failure : public std::runtime_error {
	public:
		failure(const char* msg) : std::runtime_error(msg) {}
	};

	~task_buffer() = default;
	virtual int remaining()const = 0; // Make abstract
	virtual int find_closest_length_leq(const int target)const;
	virtual int find_closest_length_geq(const int target)const;
	virtual int find_largest_length()const;
	virtual toy_task extract_task(const int len);
};


/** Implemented as a sorted list.
 */
class task_buffer_list : public task_buffer
{
public:
	task_buffer_list(const std::vector<toy_task> toys);
	task_buffer_list() {}
	void add_task(toy_task t);
	void set_for_use();

	int remaining()const { return data.size(); }
	int find_closest_length_leq(const int target)const;
	int find_closest_length_geq(const int target)const;
	int find_largest_length()const { return data.back().len; }
	toy_task extract_task(const int len);
private:
	std::vector<toy_task> data;
};


/** Implements using a std::map
 */
class task_buffer_map : public task_buffer
{
public:
	task_buffer_map(const std::vector<toy_task> toys);
	task_buffer_map();
	void add_task(toy_task t);
	void set_for_use();

	int remaining()const { return total_count; }
	int find_closest_length_leq(const int target)const;
	int find_closest_length_geq(const int target)const;
	int find_largest_length()const;
	toy_task extract_task(const int len);
	void save(std::ostream& out);
	void load(std::istream& in);
private:
	std::map<int,std::vector<toy_task>> data;
	std::vector<int> available_lengths;
	int total_count;
private:
	void generate_available_lengths();
	template <typename T>
	void write_vector(std::ostream& out, const std::vector<T>& data);
	template <typename T>
	std::vector<T> read_vector(std::istream& in);
};

/** Load task list in */

task_buffer_map load_toys_map(std::string filename);
task_buffer_list load_toys_list(std::string filename);
std::vector<toy_task> load_toys_vector(std::string filename);

#endif // __tasks_h
