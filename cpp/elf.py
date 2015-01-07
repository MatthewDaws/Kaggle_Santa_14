# This is a script, so we'll tolerate some global variables, and "magic numbers"
daystarts = 9*60
dayends = 19*60
minsday = 24*60
sanctionedminsday = dayends - daystarts
unsanctionedminsday = minsday - sanctionedminsday

import math

def ssu_spliter(duration, place, start, end):
    """Input: duration, place, start, end
    where start--end represents an interval of time, place is the current place in the
    day, and duration is the total duration.
    Intersects [place, place+duration] with the interval, and returns the 'delta' which
    is the maximum we can add to place to either get duration==0 or to reach the end of
    the interval; returns 0 if no intersection"""
    if start <= place and place < end:
        return min(duration, end - place)
    return 0

def split_sanctioned_unsanctioned(starttime, duration):
    """Split time period into sanctioned and unsanctioned minutes
    Input: starttime, integer, time since 2014 in minutes
      duration, integer, time in minutes
    Returns: (sanctioned, unsanctioned)"""
    san = 0
    unsan = 0
    # In any 24 hour period, always the same number of sanctioned and unsanctioned minutes
    # so deal with hour days first, and then with remainder
    wholedays = duration // minsday
    san += wholedays * sanctionedminsday
    unsan += wholedays * unsanctionedminsday
    duration -= wholedays * minsday
    # So now duration < A Day, but still might overlap everything
    # Only care about starttime relative to current day now
    day_starttime = starttime % minsday
    while duration > 0:
        # Before start of day
        delta = ssu_spliter(duration, day_starttime, 0, daystarts)
        unsan += delta
        day_starttime = (day_starttime + delta) % minsday
        duration -= delta
        # Working hours
        delta = ssu_spliter(duration, day_starttime, daystarts, dayends)
        san += delta
        day_starttime = (day_starttime + delta) % minsday
        duration -= delta
        # After working hours
        delta = ssu_spliter(duration, day_starttime, dayends, minsday)
        unsan += delta
        day_starttime = (day_starttime + delta) % minsday
        duration -= delta
    return (san,unsan)


class ElfExcept(Exception):
    pass

class Elf:
    def __str__(self):
        return "Elf id:{}, productivity:{}, next_free:{}".format(
            self.elfid, self.productivity, self.available_to_work)
    def __init__(self, elfid):
        self.productivity = 1.0
        self.elfid = elfid
        # Time the Elf is next able to work
        self.available_to_work = 60*9 # Work starts at 9am Jan 1st
        self.logging = False # Create a history
        if self.logging:
            self.history = []
    def do_task(self, starttime, duration):
        if starttime < self.available_to_work:
            raise ElfExcept("Elf {} not available to work at time {}.".format(self.elfid,starttime))
        # So now do all the calculation
        actual_duration = math.ceil(duration / self.productivity)
        san, unsan = split_sanctioned_unsanctioned(starttime, actual_duration)
        self.available_to_work = starttime + actual_duration
        if self.logging:
            log = "At time {} assigned task of length {} with productivity {}".format(starttime, duration, self.productivity)
            log += " which took me {}, sanctioned:{}, unsanctioned:{}".format(actual_duration, san, unsan)
        # Update productivity
        new_prod = (1.02 ** (san/60.0)) * (0.9 ** (unsan/60.0)) * self.productivity
        if new_prod < 0.25:
            new_prod = 0.25
        if new_prod > 4.0:
            new_prod = 4.0
        self.productivity = new_prod
        # Work out rest time needed
        # Copied this logic from the official Python code, because of "bug"/"feature".
        if unsan > 0:
            num_days_since_jan1 = self.available_to_work // minsday
            rest_time = unsan
            rest_time_in_working_days = rest_time // sanctionedminsday
            rest_time_remaining_minutes = rest_time % sanctionedminsday
            local_start = self.available_to_work % minsday
            if local_start < daystarts:
                local_start = daystarts
            elif local_start > dayends:
                num_days_since_jan1 += 1
                local_start = daystarts
            if local_start + rest_time_remaining_minutes > dayends:
                rest_time_in_working_days += 1
                rest_time_remaining_minutes -= (dayends - local_start)
                local_start = daystarts
            total_days = num_days_since_jan1 + rest_time_in_working_days
            self.available_to_work = total_days * minsday + local_start + rest_time_remaining_minutes
        if self.logging:
            log += " will get off rest time at {}.".format(self.available_to_work)
            self.history.append( log )
        return actual_duration


if __name__ == "__main__":
	print("Sanctioned minutes: {} -- {} which is {} out of {}.".format(daystarts,dayends,sanctionedminsday,minsday))
	print(split_sanctioned_unsanctioned(daystarts, 60*24), "should be (600, 840)")
	print(split_sanctioned_unsanctioned(10, 1000), "should be (470, 530)")
	print(split_sanctioned_unsanctioned(10, 10000), "should be (4200, 5800)")
	print(split_sanctioned_unsanctioned(dayends, 100), "should be (0, 100)")
	print(split_sanctioned_unsanctioned(dayends-50, 1000), "should be (160,840)")
	print(split_sanctioned_unsanctioned(dayends-50, 1439), "should be (599,840)")
	print()
	
	e = Elf(1)
	print(e.elfid, e.productivity, e.available_to_work, "should be 1 1.0 540")
	e.do_task(540,100)
	print(e.productivity, e.available_to_work, "should be 1.033555... 640")
	e.productivity = 4.0
	e.do_task(700,100)
	print(e.productivity, e.available_to_work, "should be 4.0 725")
	e.do_task(dayends, 840)
	print(e.productivity, e.available_to_work, "should be 2.76636... 2190")
	e.do_task(2910, 200) # 3rd Jan, 0:30 for 200 mins, takes 73 mins
	print(e.productivity, e.available_to_work, "should be 2.43353... 3493")
	e.history