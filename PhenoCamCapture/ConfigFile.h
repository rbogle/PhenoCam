/**********************************************************
	Based on code from the tutorial at
	http://www.dreamincode.net/forums/topic/183191-create-a-simple-configuration-file-parser/
	minor changes made to classes.
	Appears to have not copywrite or license restrictions

***********************************************************/

#include "stdafx.h"


class Convert
{
	static log4cxx::LoggerPtr logger;

public:
	template <typename T> static inline std::string T_to_string(T const &val) 
	{
		std::ostringstream ostr;
		ostr << val;

		return ostr.str();
	}
		
	template <typename T> static inline T string_to_T(std::string const &val) 
	{
		std::istringstream istr(val);
		T returnVal;
		if (!(istr >> returnVal)){
			LOG4CXX_ERROR(logger,"CFG: Not a valid " + (std::string)typeid(T).name() + " received!\n");
			return returnVal;
		}
		return returnVal;
	}

	template <> static inline std::string string_to_T(std::string const &val)
	{
		return val;
	}
};


class ConfigFile
{
private:
	std::map<std::string, std::string> contents;
	std::string fName;
	static log4cxx::LoggerPtr logger;

	void removeComment(std::string &line) const;
	bool onlyWhitespace(const std::string &line) const;
	bool validLine(const std::string &line) const;
	void extractKey(std::string &key, size_t const &sepPos, const std::string &line) const;
	void extractValue(std::string &value, size_t const &sepPos, const std::string &line) const;
	void extractContents(const std::string &line);
	void parseLine(const std::string &line, size_t const lineNo);
	void ExtractKeys();

public:
	ConfigFile(const std::string &fName);
	bool keyExists(const std::string &key) const;
	template <typename ValueType> inline ValueType getValueOfKey(const std::string &key, ValueType const &defaultValue = ValueType()) const	{
		if (!keyExists(key))
			return defaultValue;

		return Convert::string_to_T<ValueType>(contents.find(key)->second);
	}
};