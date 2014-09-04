#include "errors.h"

namespace cppcms { namespace templates {
	const file_position_t& error_at_line::line() const {
		return line_;
	}
	
	error_at_line::error_at_line(const std::string& msg, const file_position_t& line)
		: std::runtime_error(msg)
		, line_(line) {}  
}}
