/* 
 * File:   KeywordRawData.hpp
 * Author: kflik
 *
 * Created on March 20, 2013, 3:59 PM
 */

#ifndef RAWKEYWORD_HPP
#define	RAWKEYWORD_HPP

#include <string>
#include <utility>
#include <list>

#include "RawRecord.hpp"

namespace Opm {

    class RawKeyword {
    public:
        RawKeyword();
        RawKeyword(const std::string& keyword);
        static bool tryGetValidKeyword(const std::string& line, std::string& result);
        std::string getKeyword();
        void setKeyword(const std::string& keyword);
        void addRawRecordString(const std::string& partialRecordString);
        virtual ~RawKeyword();

    private:
        std::string m_keyword;
        std::list<RawRecord> m_records;
        std::string m_partialRecordString;
        static bool isValidKeyword(const std::string& keywordCandidate);
    };
}
#endif	/* RAWKEYWORD_HPP */

