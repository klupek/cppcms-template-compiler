#ifndef CPPCMS_TEMPLATE_COMPILER_ERRORS_H
#define CPPCMS_TEMPLATE_COMPILER_ERRORS_H
#include "parser_source.h"
#include <stdexcept>
namespace cppcms { namespace templates {
	class error_at_line : public std::runtime_error {
		const file_position_t line_;
	public:
		error_at_line(const std::string& msg, const file_position_t& line);
		const file_position_t& line() const;
	};

	class parse_error : public std::runtime_error {
		using std::runtime_error::runtime_error;
	};
	
	class bad_cast : public std::runtime_error {
	public:
		using std::runtime_error::runtime_error;
	};
}}
#endif
