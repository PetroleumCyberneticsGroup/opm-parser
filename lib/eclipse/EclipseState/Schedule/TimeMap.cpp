/*
  Copyright 2013 Statoil ASA.

  This file is part of the Open Porous Media project (OPM).

  OPM is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  OPM is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with OPM.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <ctime>

#include <ert/util/util.h>

#include <opm/parser/eclipse/Deck/Deck.hpp>
#include <opm/parser/eclipse/Deck/DeckItem.hpp>
#include <opm/parser/eclipse/Deck/DeckKeyword.hpp>
#include <opm/parser/eclipse/Deck/DeckRecord.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/TimeMap.hpp>


namespace Opm {
    TimeMap::TimeMap(std::time_t startTime) {
        m_timeList.push_back(startTime);
    }

    TimeMap::TimeMap( const Deck& deck) {

        if (deck.hasKeyword("START")) {
            // Use the 'START' keyword to find out the start date (if the
            // keyword was specified)
            const auto& keyword = deck.getKeyword("START");
            m_timeList.push_back(timeFromEclipse(keyword.getRecord(0)));

        } else {
            // The default start date is not specified in the Eclipse
            // reference manual. We hence just assume it is same as for
            // the START keyword for Eclipse R100, i.e., January 1st,
            // 1983...
            const std::time_t time = mkdate(1983, 1, 1);
            m_timeList.push_back(time);
        }

        // find all "TSTEP" and "DATES" keywords in the deck and deal
        // with them one after another
        for( const auto& keyword : deck ) {
            // We're only interested in "TSTEP" and "DATES" keywords,
            // so we ignore everything else here...
            if (keyword.name() != "TSTEP" &&
                keyword.name() != "DATES")
            {
                continue;
            }

            if (keyword.name() == "TSTEP")
                addFromTSTEPKeyword(keyword);
            else if (keyword.name() == "DATES")
                addFromDATESKeyword(keyword);
        }
    }

    size_t TimeMap::numTimesteps() const {
        return m_timeList.size() - 1;
    }


    std::time_t TimeMap::getStartTime(size_t tStepIdx) const {
        return this->operator[]( tStepIdx );
    }

    std::time_t TimeMap::getEndTime() const {
        return this->operator[]( this->size( ) - 1);
    }

    double TimeMap::getTotalTime() const
    {
        if (m_timeList.size() < 2)
            return 0.0;
        const time_t deltaT = m_timeList.back() - m_timeList.front();
        return static_cast<double>(deltaT);
    }

    void TimeMap::addTime(std::time_t newTime) {
        const std::time_t lastTime = m_timeList.back();
        const size_t step = m_timeList.size();
        if (newTime > lastTime) {
            int new_day, new_month, new_year, last_day, last_month, last_year;
            util_set_date_values_utc(newTime, &new_day, &new_month, &new_year);
            util_set_date_values_utc(lastTime, &last_day, &last_month, &last_year);

            if (new_month != last_month)
                m_first_timestep_months.push_back(step);

            if (new_year != last_year)
                m_first_timestep_years.push_back(step);

            m_timeList.push_back(newTime);
        } else
            throw std::invalid_argument("Times added must be in strictly increasing order.");
    }

    void TimeMap::addTStep(int64_t step) {
        addTime(forward(m_timeList.back(), step));
    }

    size_t TimeMap::size() const {
        return m_timeList.size();
    }

    size_t TimeMap::last() const {
        return this->numTimesteps();
    }

    const std::map<std::string , int>& TimeMap::eclipseMonthIndices() {
        static std::map<std::string , int> monthIndices;

        if (monthIndices.size() == 0) {
            monthIndices.insert( std::make_pair( "JAN" , 1 ));
            monthIndices.insert( std::make_pair( "FEB" , 2 ));
            monthIndices.insert( std::make_pair( "MAR" , 3 ));
            monthIndices.insert( std::make_pair( "APR" , 4 ));
            monthIndices.insert( std::make_pair( "MAI" , 5 ));
            monthIndices.insert( std::make_pair( "MAY" , 5 ));
            monthIndices.insert( std::make_pair( "JUN" , 6 ));
            monthIndices.insert( std::make_pair( "JUL" , 7 ));
            monthIndices.insert( std::make_pair( "JLY" , 7 ));
            monthIndices.insert( std::make_pair( "AUG" , 8 ));
            monthIndices.insert( std::make_pair( "SEP" , 9 ));
            monthIndices.insert( std::make_pair( "OCT" , 10 ));
            monthIndices.insert( std::make_pair( "OKT" , 10 ));
            monthIndices.insert( std::make_pair( "NOV" , 11 ));
            monthIndices.insert( std::make_pair( "DEC" , 12 ));
            monthIndices.insert( std::make_pair( "DES" , 12 ));
        }

        return monthIndices;
    }


    std::time_t TimeMap::timeFromEclipse(const DeckRecord &dateRecord) {
        const auto &dayItem = dateRecord.getItem(0);
        const auto &monthItem = dateRecord.getItem(1);
        const auto &yearItem = dateRecord.getItem(2);
        const auto &timeItem = dateRecord.getItem(3);

        int hour = 0, min = 0, second = 0;
        if (timeItem.hasValue(0)) {
            if (sscanf(timeItem.get<std::string>(0).c_str(), "%d:%d:%d" , &hour,&min,&second) != 3) {
                hour = min = second = 0;
            }
        }

        std::time_t date = mkdatetime(yearItem.get<int>(0),
                                      eclipseMonthIndices().at(monthItem.get<std::string>(0)),
                                      dayItem.get<int>(0),
                                      hour,
                                      min,
                                      second);
        return date;
    }

    void TimeMap::addFromDATESKeyword(const DeckKeyword &DATESKeyword) {
        if (DATESKeyword.name() != "DATES")
            throw std::invalid_argument("Method requires DATES keyword input.");

        for (size_t recordIndex = 0; recordIndex < DATESKeyword.size(); recordIndex++) {
            const auto &record = DATESKeyword.getRecord(recordIndex);
            const std::time_t nextTime = TimeMap::timeFromEclipse(record);
            addTime(nextTime);
        }
    }

    void TimeMap::addFromTSTEPKeyword(const DeckKeyword &TSTEPKeyword) {
        if (TSTEPKeyword.name() != "TSTEP")
            throw std::invalid_argument("Method requires TSTEP keyword input.");
        {
            const auto &item = TSTEPKeyword.getRecord(0).getItem(0);

            for (size_t itemIndex = 0; itemIndex < item.size(); itemIndex++) {
                const double days = item.get<double>(itemIndex);
                const int64_t seconds = static_cast<int64_t>(days * 24 * 60 * 60);
                addTStep(seconds);
            }
        }
    }

    double TimeMap::getTimeStepLength(size_t tStepIdx) const
    {
        assert(tStepIdx < numTimesteps());
        const std::time_t t1 = m_timeList[tStepIdx];
        const std::time_t t2 = m_timeList[tStepIdx + 1];
        const std::time_t deltaT = t2 - t1;
        return static_cast<double>(deltaT);
    }

    double TimeMap::getTimePassedUntil(size_t tLevelIdx) const
    {
        assert(tLevelIdx < m_timeList.size());
        const std::time_t t1 = m_timeList.front();
        const std::time_t t2 = m_timeList[tLevelIdx];
        const std::time_t deltaT = t2 - t1;
        return static_cast<double>(deltaT);
    }


    bool TimeMap::isTimestepInFirstOfMonthsYearsSequence(size_t timestep, bool years, size_t start_timestep, size_t frequency) const {
        bool timestep_first_of_month_year = false;
        const std::vector<size_t>& timesteps = (years) ? getFirstTimestepYears() : getFirstTimestepMonths();

        std::vector<size_t>::const_iterator ci_timestep = std::find(timesteps.begin(), timesteps.end(), timestep);
        if (ci_timestep != timesteps.end()) {
            if (1 >= frequency) {
                timestep_first_of_month_year = true;
            } else { //Frequency given
                timestep_first_of_month_year = isTimestepInFreqSequence(timestep, start_timestep, frequency, years);
            }
        }
        return timestep_first_of_month_year;
    }


    // This method returns true for every n'th timestep in the vector of timesteps m_first_timestep_years or m_first_timestep_months,
    // starting from one before the position of start_timestep. If the given start_timestep is not a value in the month or year vector,
    // set the first timestep that are both within the vector and higher than the initial start_timestep as new start_timestep.

    bool TimeMap::isTimestepInFreqSequence (size_t timestep, size_t start_timestep, size_t frequency, bool years) const {
        bool timestep_right_frequency = false;
        const std::vector<size_t>& timesteps = (years) ? getFirstTimestepYears() : getFirstTimestepMonths();

        std::vector<size_t>::const_iterator ci_timestep = std::find(timesteps.begin(), timesteps.end(), timestep);
        std::vector<size_t>::const_iterator ci_start_timestep = std::find(timesteps.begin(), timesteps.end(), start_timestep);

        //Find new start_timestep if the given one is not a value in the timesteps vector
        bool start_ts_in_timesteps = false;
        if (ci_start_timestep != timesteps.end()) {
            start_ts_in_timesteps = true;
        } else if (ci_start_timestep == timesteps.end()) {
            size_t new_start = closest(timesteps, start_timestep);
            if (0 != new_start) {
                ci_start_timestep = std::find(timesteps.begin(), timesteps.end(), new_start);
                start_ts_in_timesteps = true;
            }
        }


        if (start_ts_in_timesteps) {
            //Pick every n'th element, starting on start_timestep + (n-1), that is, every n'th element from ci_start_timestep - 1 for freq n > 1
            if (ci_timestep >= ci_start_timestep) {
                int dist = std::distance( ci_start_timestep, ci_timestep ) + 1;
                if ((dist % frequency) == 0) {
                    timestep_right_frequency = true;
                }
            }
        }

        return timestep_right_frequency;
    }





    const std::vector<size_t>& TimeMap::getFirstTimestepMonths() const {
        return m_first_timestep_months;
    }


    const std::vector<size_t>& TimeMap::getFirstTimestepYears() const {
        return m_first_timestep_years;
    }

    // vec is assumed to be sorted
    size_t TimeMap::closest(const std::vector<size_t> & vec, size_t value) const
    {
        std::vector<size_t>::const_iterator ci =
            std::lower_bound(vec.begin(), vec.end(), value);
        if (ci != vec.end()) {
            return *ci;
        }
        return 0;
    }


    std::time_t TimeMap::operator[] (size_t index) const {
        if (index < m_timeList.size()) {
            return m_timeList[index];
        } else
            throw std::invalid_argument("Index out of range");
    }


    std::time_t TimeMap::mkdate(int in_year, int in_month, int in_day) {
        return mkdatetime(in_year , in_month , in_day, 0,0,0);
    }

    std::time_t TimeMap::mkdatetime(int in_year, int in_month, int in_day, int hour, int minute, int second) {
        std::time_t t = util_make_datetime_utc(second, minute, hour, in_day, in_month, in_year);
        {
            /*
               The underlying mktime( ) function will happily wrap
               around dates like January 33, this function will check
               that no such wrap-around has taken place.
            */
            int out_year, out_day, out_month;
            util_set_date_values_utc( t, &out_day , &out_month, &out_year);
            if ((in_day != out_day) || (in_month != out_month) || (in_year != out_year))
                throw std::invalid_argument("Invalid input arguments for date.");
        }
        return t;
    }


    std::time_t TimeMap::forward(std::time_t t, int64_t seconds) {
        return t + seconds;
    }

    std::time_t TimeMap::forward(std::time_t t, int64_t hours, int64_t minutes, int64_t seconds) {
        return t + seconds + minutes * 60 + hours * 3600;
    }
}


