#include "parser_source.h"

#include <stdexcept>
#include <fstream>
namespace cppcms { namespace templates {

	std::string readfile(const std::string& filename) {
		std::ifstream ifs(filename);
		if(!ifs)
			throw std::runtime_error("unable to open file '" + filename + "'");

		return std::string(std::istreambuf_iterator<char>{ifs}, std::istreambuf_iterator<char>{});
	}
	

	std::pair<std::string, std::vector<file_index_t>> readfiles(const std::vector<std::string>& files) {
		std::string content;
		std::vector<file_index_t> indexes;
		size_t all_lines = 0;
		for(const std::string& fn : files) {
			std::string part = readfile(fn);
			if(part[part.length()-1] != '\n')
				part += '\n';

			size_t lines = 0;
			for(const char c : part)
				if(c == '\n')
					lines ++;

			indexes.push_back({fn, content.length(), content.length()+part.length(), all_lines, all_lines+lines});
			all_lines += lines;
			content += part;
		}
		return std::make_pair(content, indexes);
	}

	parser_source::parser_source(const std::vector<std::string>& files)
		: input_pair_(readfiles(files))
		, input_(input_pair_.first)
       		, index_(0)
		, line_(1) 
		, file_indexes_(input_pair_.second) {}

	void parser_source::reset(size_t index, file_position_t line) {
		line_ = line.line;
		index_ = index;
	}

	std::string parser_source::left_context_from(size_t beg) const { 		
		return slice(beg, index_); 
	}
	
	std::string parser_source::right_context_to(size_t end) const { 		
		return slice(index_, end); 
	}

	std::string parser_source::slice(size_t beg, size_t end) const { 
		return input_.substr(beg, (end < input_.length() ? end-beg : input_.length()-beg)); 
	}

	size_t parser_source::length() const { 
		return input_.length(); 
	}

	bool parser_source::compare(size_t beg, const std::string& other) const {
		return (beg + other.length() <= input_.length() 
				&& input_.compare(beg, other.length(), other) == 0);
	}

	bool parser_source::compare_head(const std::string& other) const { return compare(index_, other); }
	std::string parser_source::substr(size_t beg, size_t len) const { return input_.substr(beg, len); }
	char parser_source::next() {
		move(1);
		return current();
	}

	char parser_source::current() const { 
		return input_[index_];
	}

	void parser_source::move(int offset) {
		if(offset + index_ > input_.length())
			throw std::logic_error("move(): offset too big");
		else if(offset + static_cast<int>(index_) < 0)
			throw std::logic_error("move(): offset too small");
		
		if(offset > 0) {
			for(size_t i = index_; i < index_+offset; ++i) {
				if(input_[i] == '\n')
					line_++;
			}
		} else {
			for(size_t i = index_+offset; i < index_; ++i) {
				if(input_[i] == '\n')
					line_--;
			}
		}
		index_ += offset;
	}

	void parser_source::move_to(size_t pos) {
		int offset = pos - index_;
		move(offset);
	}

	std::string parser_source::right_context(size_t length) const {
		length = std::min(length, input_.length()-index_);
		return input_.substr(index_, length);
	}

	std::string parser_source::left_context(size_t length) const {
		length = std::min(index_, length);
		return input_.substr(index_-length, length);
	}

	std::string parser_source::right_until_end() const {
		return input_.substr(index_);
	}

	size_t parser_source::find_on_right(const std::string& what) const {
		return input_.find(what, index_);
	}

	bool parser_source::has_next() const {
		return index_ < input_.length();
	}

	size_t parser_source::index() const { 
		return index_; 
	}

	void parser_source::mark() {
		marks_.push(index_);
#ifdef PARSER_DEBUG
		std::cerr << "mark " << marks_.size() << " at " << right_context(20) << std::endl;
#endif
	}
	
	void parser_source::unmark() {
#ifdef PARSER_DEBUG
		std::cerr << "unmark " << marks_.size() << " at " << slice(marks_.top(), marks_.top()+20) << std::endl;
#endif
		marks_.pop();
	}

	size_t parser_source::get_mark() const {
		return marks_.top();
	}

	std::string parser_source::right_from_mark() {
		auto result = left_context_from(marks_.top());
#ifdef PARSER_DEBUG
		std::cerr << "mark2text " << marks_.size() << " at " << slice(marks_.top(), marks_.top()+20) << std::endl;
#endif
		marks_.pop();
		return result;
	}

	file_position_t parser_source::line() const {
		for(const file_index_t& fi : file_indexes_) {
			if(index_ >= fi.beg && index_ < fi.end) {
				return file_position_t { fi.filename, line_ - fi.line_beg };
			}
		}
		if(!file_indexes_.empty() && index_ == file_indexes_.back().end) 
			return file_position_t { file_indexes_.back().filename, line_ - file_indexes_.back().line_beg };

		throw std::logic_error("bug: file index not found");
	}
}}
