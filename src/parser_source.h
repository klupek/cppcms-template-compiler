#ifndef CPPCMS_TEMPLATES_COMPILER_PARSER_SOURCE_H
#define CPPCMS_TEMPLATES_COMPILER_PARSER_SOURCE_H
#include <string>
#include <utility>
#include <vector>
#include <stack>
namespace cppcms { namespace templates {
	struct file_position_t {
		std::string filename;
		size_t line;
	};

	struct file_index_t {
		const std::string filename;
		const size_t beg, end;
		const size_t line_beg, line_end;
	};

	class parser_source {
		const std::pair<std::string, std::vector<file_index_t>> input_pair_;
		const std::string input_;
		size_t index_, line_;
		std::vector<file_index_t> file_indexes_;
		std::stack< size_t > marks_;
	public:
		parser_source(const std::vector<std::string>& files);
		void reset(size_t index, file_position_t line);

		void move(int offset); // index_ += index_offset
		void move_to(size_t pos); // index_ = pos;
		bool has_next() const; // index_ < length
		char current() const;
		char next(); // index_++; return input_[index_];
		size_t index() const;
		file_position_t line() const;
		std::string substr(size_t beg, size_t len) const;
		bool compare_head(const std::string& other) const; // as below && [index_, index_+other.length()] == other
		bool compare(size_t beg, const std::string& other) const; // .length() - index_ >= token.length() && compare
		size_t length() const;
		std::string slice(size_t beg, size_t end) const; // [beg...end-1]

		// get substring, all characters, starting from current index_
		std::string right_until_end() const; // slice(index_, length())

		// find first token on the right side of current index_ (including current character)
		size_t find_on_right(const std::string& token) const;


		// get up to n chars right from current, including current
		std::string right_context(size_t length) const;

		// get up to n chars left from current, without current
		std::string left_context(size_t length) const;
		
		// get up to n chars right from current, including current
		std::string right_context_to(size_t end) const;

		// get up to n chars left from current, without current
		std::string left_context_from(size_t beg) const;

		void mark();
		void unmark();
		size_t get_mark() const;
		std::string right_from_mark();
	};
}}
#endif
