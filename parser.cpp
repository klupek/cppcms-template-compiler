#include "parser.h"
#include <fstream>
namespace cppcms { namespace templates {
	static bool is_latin_letter(char c) {
		return ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' );
	}

	static bool is_digit(char c) {
		return ( c >= '0' && c <= '9' );
	}

	struct ln { 
		const file_position_t line_;
		ln(file_position_t line) : line_(line) {}
	};

	error_at_line::error_at_line(const std::string& msg, const file_position_t& line)
		: std::runtime_error(msg)
		, line_(line) {}  

	const file_position_t& error_at_line::line() const {
		return line_;
	}

	static std::ostream& operator<<(std::ostream&o, const ln& obj) {
		return o << "#line " << obj.line_.line << " \"" << obj.line_.filename << "\"\n";
	}

	static std::string trim(const std::string& input) {
		size_t beg = 0, end = input.length();
		
		while(beg < input.length() && std::isspace(input[beg]))
			++beg;

		while(end > beg && std::isspace(input[end-1]))
			--end;
		return std::string(&input[beg], &input[end]);
	}

	static bool is_whitespace_string(const std::string& input) {
		return std::all_of(input.begin(), input.end(), [](char c) {
			return std::isspace(c);
		});
	}

	static std::string decode_escaped_string(const std::string& input) {
		std::string result;
		bool escaped = false;
		const int hex[256] = 
			{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
			  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
			  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
			  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, -1, -1, -1, -1, -1, -1, 
			  -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
			  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
			  -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
			  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
			  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
			  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
			  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
			  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
			  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
			  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
			  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
			  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
		for(size_t i = 0; i < input.length(); ++i) {
			char current = input[i];
			if(escaped) {
				escaped = false;
				int value;
				char o1,o2,o3;
				switch(current) {					
					case '\'':
					case '?':
					case '\\':
						result += current;
						break;
					case '"': result += "\\x22"; break;
					case 'a': result += '\a'; break;
					case 'b': result += '\b'; break;
					case 'f': result += '\f'; break;
					case 'n': result += '\n'; break;
					case 'r': result += '\r'; break;
					case 't': result += '\t'; break;
					case 'v': result += '\v'; break;
							  							  
					case 'x':
						  if(i+2 >= input.length())
							  throw std::runtime_error("Invalid escape sequence: " + std::string(&input[i-1], &input[input.length()-1]));
						  value = hex[static_cast<int>(input[i+1])]*16 + hex[static_cast<int>(input[i+2])];
						  if(value < 0)
							  throw std::runtime_error("Invalid escape sequence: " + std::string(&input[i-1], &input[i+3]));
						  result += static_cast<char>(value);
						  break;
					// TODO: unicode characters
					default:
						if(input[i] >= '0' && input[i] <= '7') {
							if(i+3 >= input.length())
								throw std::runtime_error("Invalid escape sequence: " + std::string(&input[i-1], &input[input.length()-1]));
							o1 = input[i+1], o2 = input[i+2], o3 = input[i+3];
							if(o2 >= '0' && o2 <= '7' && o3 >= '0' && o3 <= '7') {
								o1 -= '0'; o2 -= '0'; o3 -= '0';
								result += static_cast<char>(o1*64+o2*8+o3);
							} else {
								throw std::runtime_error("Invalid escape sequence: " + std::string(&input[i-1], &input[i+4]));
							}
						} else {
							result += '\\';
							result += current;
						}
						break;
				}
			} else if(input[i] == '\\') {
				escaped = true;
			} else {
				result += current;
			}
		}
		if(escaped)
			result += '\\';
		return result;
	}

	
	static std::string compress_html(const std::string& input) {
		std::string result;
		char translate[255] = { 0 };
		translate[static_cast<unsigned int>('\a')] = 'a';
		translate[static_cast<unsigned int>('\b')] = 'b'; 
		translate[static_cast<unsigned int>('\f')] = 'f';
		translate[static_cast<unsigned int>('\n')] = 'n';
		translate[static_cast<unsigned int>('\r')] = 'r';
		translate[static_cast<unsigned int>('\t')] = 't';
		translate[static_cast<unsigned int>('\v')] = 'v';
		for(const char c : input) {
			if(c == '"') {
				result += "\\x22";
			} else if(c == '\\') {
				result += '\\';
				result += c;
			} else if(translate[static_cast<unsigned char>(c)] > 0) {
				result += '\\';
				result += translate[static_cast<unsigned char>(c)];
			} else { // if(std::isprint(c)) {
				result += c;
			}
		}
		return result;
	}
	
	static std::string compress_string(const std::string& input) {
		std::string result = "\"";
		char translate[255] = { 0 };
		translate[static_cast<unsigned int>('\a')] = 'a';
		translate[static_cast<unsigned int>('\b')] = 'b'; 
		translate[static_cast<unsigned int>('\f')] = 'f';
		translate[static_cast<unsigned int>('\n')] = 'n';
		translate[static_cast<unsigned int>('\r')] = 'r';
		translate[static_cast<unsigned int>('\t')] = 't';
		translate[static_cast<unsigned int>('\v')] = 'v';
		for(size_t i = 1; i < input.length()-1; ++i) {
			const char c = input[i];
			if(c == '\\' && input[i+1] == '"') {
				result += "\\x22";
				++i;
			} else if(c == '\\') {
				result += '\\';
				result += c;
			} else if(translate[static_cast<unsigned char>(c)] > 0) {
				result += '\\';
				result += translate[static_cast<unsigned char>(c)];
			} else { // if(std::isprint(c)) {
				result += c;
			}
		}
		result += '"';
		return result;		
	}

	static expr::ptr recognize_expr(const std::string& input) {
		const std::string trimmed = trim(input);
		if(trimmed[0] == '"')
			return expr::make_string(trimmed);
		else if(trimmed.length() >= 3 && trimmed[0] == '0' && trimmed[1] == 'x')
			return expr::make_number(trimmed);
		else if(std::all_of(trimmed.begin(), trimmed.end(), [](char c) {
			return c == '-' || c == '.' || c == '+' || (c >= '0' && c <= '9');
		})) {
			return expr::make_number(trimmed);
		} else
			return expr::make_variable(trimmed);
	}

	static std::pair<std::string, std::vector<expr::ptr>> split_function_call(const std::string& call) {
		typedef std::pair<std::string, std::vector<expr::ptr>> result_t;
		result_t result;
		size_t beg = call.find("("), end = call.length();
		if(beg == std::string::npos) {
			result.first = call;
			return result;
		} else {
			result.first = std::string(&call[0], &call[beg]);
			// parse c++ expression
			// bracket counts, a = (), b = [], c = <>
			int brackets_a = 0, brackets_b = 0, brackets_c = 0;
			// in string
			bool string = false, escaped = false;
			size_t next = beg+1;

			for(size_t i = beg+1; i < end-1; ++i) {
				const char c = call[i];
				if((brackets_a == 0 && brackets_b == 0 && brackets_c == 0 
					&& !string && c == ',') || i == end-1) {
					const std::string arg(&call[next], &call[i]);					
					result.second.emplace_back(recognize_expr(arg));
					next = i+1;
				} else if(c == '(' && !string) 
					brackets_a++;
				else if(c == ')' && !string)
					brackets_a--;
				else if(c == '[' && !string)
					brackets_b++;
				else if(c == ']' && !string)
					brackets_b--;
				else if(c == '<' && !string)
					brackets_c++;
				else if(c == '>' && !string)
					brackets_c--;
				else if(c == '"' && !string) 
					string = true;
				else if(c == '"' && string && !escaped)
					string = false;
				else if(c == '\\' && string && !escaped)
					escaped = true;
				else {
					escaped = false;
				}
			}
			result.second.emplace_back(recognize_expr(std::string(&call[next], &call[end-1])));
			return result;
		}
	}

	std::string readfile(const std::string& filename) {
		std::ifstream ifs(filename);
		if(!ifs)
			throw std::runtime_error("unable to open file '" + filename + "'");

		return std::string(std::istreambuf_iterator<char>{ifs}, std::istreambuf_iterator<char>{});
	}
	
	std::string translate_ast_object_name(const std::string& name) {
		const std::string prefix = "cppcms::templates::ast::";
		const std::string suffix = "_t";
		if(name == "cppcms::templates::ast::root_t")
			return "skin";
		else if(name == "cppcms::templates::ast::has_children_t")
			return "(any block node, like template, if, foreach, ...)";
		else if(name == "cppcms::templates::ast::cppcode_t")
			return "c++";
		else if(name == "cppcms::templates::ast::fmt_function_t")
			return "(format function, like gt, url, ...)";
		else if(name == "cppcms::templates::ast::if_t::condition_t")
			return "if";
		else if(name == "cppcms::templates::ast::foreach_t::part_t")
			return "foreach child (item, separator, empty, prefix, suffix)";
		else if(name.compare(0, prefix.length(), prefix) == 0 && name.compare(name.length() - suffix.length(), suffix.length(), suffix) == 0)
			return name.substr(prefix.length(), name.length()-prefix.length()-suffix.length());		
		else 
			return name;

	}

	namespace generator {
		void context::add_scope_variable(const std::string& name) {
			if(!scope_variables.insert(name).second)
				throw std::runtime_error("duplicate local scope variable: " + name);
		}
		
		void context::remove_scope_variable(const std::string& name) {
			if(!scope_variables.erase(name))
				throw std::logic_error("bug: tried to remove variable: " + name + " which is notin local scope");
		}
			
		bool context::check_scope_variable(const std::string& name) {
			return scope_variables.find(name) != scope_variables.end();
		}
		
		void context::add_include(const std::string& filename) {
			includes.insert(filename);
		}

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

	file_position_t parser::line() const { return source_.line(); }

	token_sink::token_sink(std::string& dst)
		: target_(&dst)
		, details_(new std::stack<detail_t>()) {}

	token_sink::token_sink() 
		: target_(&tmp_) 
		, details_(new std::stack<detail_t>()) {}

	void token_sink::put(const std::string& what) {
		*target_ = what;
	}

	bool token_sink::has_details() const {
		return !details_->empty();
	}

	void token_sink::add_detail(const std::string& what, const std::string& item) {
		details_->push({what, item});
	}
		
	const std::string& token_sink::value() const {
		return *target_;
	}

	token_sink::detail_t token_sink::get_detail() {
		if(details_->empty())
			throw std::logic_error("bug: details empty");

		detail_t result = details_->top();
		details_->pop();
		return result;
	}
		
	const token_sink::detail_t& token_sink::top_detail() const {
		if(details_->empty())
			throw std::logic_error("bug: details empty");

		return details_->top();
	}

	parser::parser(const std::vector<std::string>& files)
		: source_(files)
       		, failed_(0) {}
	
	parser& parser::try_token(const std::string& token) {
#ifdef PARSER_DEBUG
		std::cout << ">>>(" << failed_ << ") find token '" << token << "' at '" << source_.right_context(20) << "'\n";
#endif
		if(!failed_ && source_.compare_head(token)) {
			stack_.emplace_back(state_t { source_.index() });
#ifdef PARSER_DEBUG
			std::cout << ">>> token " << token << std::endl;
#endif
			source_.move(token.length());
		} else {
			failed_ ++;
		}
		return *this;
	}

	parser& parser::try_one_of_tokens(const std::vector<std::string>& tokens, token_sink out) {
		if(!failed_) {
			auto i = tokens.begin();
			try_token(*i++);
			while(failed_ && i != tokens.end()) {
				back(1).try_token(*i++);
			}
			if(!failed_)
				out.put(*--i);
		} else {
			failed_++;
		}
		return *this;
	}
	
	parser& parser::try_close_expression() {
		if(!failed_) {
			push();

			skipws(false);
			if(!try_token("%>")) {
				back(1).try_token("% >");
			}
			compress();
			pop();
		} else {
			failed_++;
		}

		return *this;
	}

	/* NAME is a sequence of Latin letters, digits and underscore starting with a letter. They represent identifiers and can be defined by regular expression such as: [a-zA-Z][a-zA-Z0-9_]* */
	parser& parser::try_name(token_sink out) {
#ifdef PARSER_DEBUG
		std::cout << ">>>(" << failed_ << ") find name at '" << source_.right_context(20) << "'\n";
#endif
		if(!failed_ && source_.has_next()) {
			source_.mark();
			char c = source_.current();
			if(is_latin_letter(c) || c == '_') {
				for(;source_.has_next() && ( is_latin_letter(c) || is_digit(c) || c == '_'); c = source_.next());
				stack_.emplace_back(state_t { source_.get_mark() });
				out.put(source_.right_from_mark());
#ifdef PARSER_DEBUG
				std::cout << ">>> name " << out.value() << std::endl;
#endif
			} else {
				source_.unmark();
				failed_ ++;
			}
		} else {
			failed_++;
		}
		return *this;
	}

	parser& parser::try_string(token_sink out) {
#ifdef PARSER_DEBUG
		std::cout << ">>>(" << failed_ << ") find string at '" << source_.right_context(20) << "'\n";
#endif
		if(!failed_ && source_.has_next()) {
			source_.mark();
			char c = source_.current();
			if(c == '"') {
				bool escaped = false;
				c = source_.next();
				for(; source_.has_next() && (c != '"' || escaped); c = source_.next()) {
					if(escaped)
						escaped = false;
					else if(c == '\\') 
						escaped = true;
				}
				if(!source_.has_next()) { // '"' was not found
					source_.move_to(source_.get_mark());
					raise("expected \", found EOF instead");
				} else {
					source_.next();
					stack_.emplace_back(state_t { source_.get_mark() }); 
					out.put(source_.right_from_mark());
#ifdef PARSER_DEBUG
					std::cout << ">>> string " << out.value() << std::endl;
#endif
				}
			} else {
#ifdef PARSER_DEBUG
				std::cout << ">>> string failed at " << source_.index() << std::endl;
#endif
				source_.unmark();
				failed_++;
			}
		} else {
			failed_++;
		}
		return *this;
	}

	// NUMBER is a number -- sequence of digits that may start with - and include .. It can be defined by the regular expression: \-?\d+(\.\d*)?
	parser& parser::try_number(token_sink out) {		
#ifdef PARSER_DEBUG
		std::cout << ">>>(" << failed_ << ") find number at '" << source_.right_context(20) << "'\n";
#endif
		if(!failed_ && source_.has_next()) {
			source_.mark();
			char c = source_.current();

			if(c == '-' || c == '+')
				c = source_.next();
			bool hex = false;
			if(c == '0' && source_.has_next()) {
				if(source_.next() == 'x') {
					hex = true;
					if(source_.has_next()) {	
						c = source_.next(); // and if there is no next, c has value 'x' and next condition fails
					} else {
						hex = false;
					}
				} else {
					source_.move(-1);
				}
			}

			if((c >= '0' && c <= '9') || (hex && c >= 'a' && c <= 'f') || (hex && c >= 'A' && c <= 'F') ) {
				bool dot = false;
				for(; (c >= '0' && c <= '9') || (!dot && c == '.') || (hex && c >= 'a' && c <= 'f') || (hex && c >= 'A' && c <= 'F'); c = source_.next()) {
					if(c == '.')
						dot = true;
				}
				stack_.emplace_back(state_t{ source_.get_mark() });				
				out.put(source_.right_from_mark());
#ifdef PARSER_DEBUG
				std::cout << ">>> number " << out.value() << std::endl;
#endif
			} else {
				source_.unmark();
				failed_++;
			}
		} else { 
			failed_++;
		}
		return *this;
	}

	// VARIABLE is non-empty sequence of NAMES separated by dot "." or "->" that may optionally end with () or begin with * for 
	// identification of function call result. No blanks are allowed. For example: data->point.x, something.else() *foo.bar.	
	parser& parser::try_variable(token_sink out) {
#ifdef PARSER_DEBUG
		std::cout << ">>>(" << failed_ << ") find var at '" << source_.right_context(20) << "'\n";
#endif
		if(!failed_ && source_.has_next()) {
			push();
			source_.mark();

			char c = source_.current();
			// parse *name((\.|->)name)
			if(c == '*')
				c = source_.next();
			if(try_name().try_argument_list(out) || back(1)) {
				// scan for ([.]|->)(NAME) blocks 
				while(skipws(false).try_one_of_tokens({".","->"}).skipws(false).try_name().try_token("()") || back(1));

				// back from 4 failed attempts(ws, token, ws, name)
				back(4);

				// add optional "()" token
				if(!try_token("()"))
					back(1);

				// return result
				out.put(source_.right_from_mark());
#ifdef PARSER_DEBUG
				std::cout << ">>> var " << out.value() << std::endl;
#endif
			} else {
				source_.unmark();
			}
			compress();
			pop();

		} else {
			failed_ ++;
		}
		return *this;
	}

	parser& parser::try_complex_variable(token_sink out) {
#ifdef PARSER_DEBUG
		std::cout << ">>>(" << failed_ << ") find cvar at '" << source_.right_context(20) << "'\n";
#endif
		if(!failed_ && source_.has_next()) {
			push();
			source_.mark();
			std::string var;
			if(try_variable(var)) {
				std::string filter;
				while(skipws(false).try_token("|").skipws(false).try_filter(filter)) { 
					out.add_detail("complex_variable", filter);
				}
				
				back(4);
								
				out.put(source_.right_from_mark());
				out.add_detail("complex_variable_name", var);
#ifdef PARSER_DEBUG
				std::cout << ">>> cvar " << out.value() << std::endl;
#endif
			} else {
				source_.unmark();
			}
			compress();
			pop();
		} else {
			failed_++;
		}
		return *this;
	}	

	// IDENTIFIER is a sequence of NAME separated by the symbol ::. No blanks are allowed. For example: data::page
	parser& parser::try_identifier(token_sink out) {
#ifdef PARSER_DEBUG
		std::cout << ">>>(" << failed_ << ") find id at '" << source_.right_context(20) << "'\n";
#endif
		if(!failed_ && source_.has_next()) {
			push();
			source_.mark();

			if(try_name()) {
				while(try_token("::").try_name());
				back(2);
			
				out.put(source_.right_from_mark());
#ifdef PARSER_DEBUG
				std::cout << ">>> id " << out.value() << std::endl;
#endif
			} else {
				source_.unmark();
			}
			compress();
			pop();
		} else {
			failed_++;
		}
		return *this;
	}

	parser& parser::try_param_list(token_sink out) {
		if(!failed_ && source_.has_next()) {
			push();
			source_.mark();
			if(try_token("(")) {
				while(!failed_) {
					bool is_const = false, is_ref = false;
					std::string name, type;
					skipws(false);				
					if(try_token(")")) {
						break;
					} else if(back(1).try_identifier_ws(type)) {
						if(try_token("const")) {
							is_const = true;
							skipws(false);
						} else {
							back(1);
						}
						if(try_token("&")) {
							is_ref = true;
							skipws(false);
						} else {
							back(1);
						}

						if(try_name(name)) {
							// do not order, or at least leave "param_end" first, as it is used as terminator
							out.add_detail( "param_end", "" );
							out.add_detail("is_ref", (is_ref ? "ref" : "") );
							out.add_detail("is_const", (is_const ? "const" : "") );
							out.add_detail("name", name);
							out.add_detail("type", type);
						} else {
							raise("expected NAME");
						}
						if(skipws(false).try_token(",")) {
						} else if(back(1).try_token(")")) {
							back(1); // next iterator will consume this token and break
						} else {
							raise("expected ',' or ')'");
						}
					} else {
						raise("expected IDENTIFIER");
					} // end if find identifier
				} // end while not failed
			} else {
				raise("expected '('"); 
			} // end if try_token("(")
			
			compress();
			pop();
			out.put(source_.right_from_mark());
		} else {
			failed_++;
		}
		return *this;
	}
	parser& parser::try_argument_list(token_sink out) {
		if(!failed_ && source_.has_next()) {		
			push();
			source_.mark();

			if(try_token("(")) {
				if(!skipws(false).try_token(")")) {
					back(2);
					bool closed = false;
					while(!closed) {
						std::string tmp;

						skipws(false);
						if(try_variable(tmp)) {
							out.add_detail("argument_variable", tmp);
						} else if(back(1).try_string(tmp)) {
							out.add_detail("argument_string", tmp);
						} else if(back(1).try_number(tmp)) {
							out.add_detail("argument_number", tmp);
						} else {
							raise("expected ')', string, number or variable");
						}

						if(try_token(")")) {
							break;
						} else if(!back(1).skipws(false).try_token(",")) {
							raise("expected ','");
						}
					}
				}
			} else {
				back(1);
			}
			
			compress();
			pop();
			out.put(source_.right_from_mark());
		} else {
			failed_++;
		}
		return *this;

	}
	// [ 'ext' ] NAME [ '(' ( VARIABLE | STRING | NUMBER ) [ ',' ( VARIABLE | STRING | NUMBER ) ] ... ]
	parser& parser::try_filter(token_sink out) {
#ifdef PARSER_DEBUG
		std::cout << ">>>(" << failed_ << ") find filter at '" << source_.right_context(20) << "'\n";
#endif
		if(!failed_ && source_.has_next()) {
			push();
			source_.mark();
			if(!try_token_ws("ext")) {
				back(1);
			}
			if(try_name()) {
				try_argument_list();
				out.put(source_.right_from_mark());
#ifdef PARSER_DEBUG
				std::cout << ">>> filter " << out.value() << std::endl;
#endif
			} else {
				source_.unmark();
				failed_++;
			}
			pop();
			compress();
		} else {
			failed_ ++;
		}
		return *this;
	}

	parser& parser::try_name_ws(token_sink out) {
		push();
		try_name(out);
		skipws(true);
		compress();
		pop();
		return *this;
	}
	
	parser& parser::try_string_ws(token_sink out) {
		push();
		try_string(out);
		skipws(true);
		compress();
		pop();
		return *this;
	}
	
	parser& parser::try_number_ws(token_sink out) {
		push();
		try_number(out);
		skipws(true);
		compress();
		pop();
		return *this;
	}

	parser& parser::try_variable_ws(token_sink out) {
		push();
		try_variable(out);
		skipws(true);
		compress();
		pop();
		return *this;
	}

	parser& parser::try_identifier_ws(token_sink out) {
		push();
		try_identifier(out);
		skipws(true);
		compress();
		pop();
		return *this;
	}

	parser& parser::skip_to(const std::string& token, token_sink out) {
		if(!failed_ && source_.has_next()) {
			size_t r = source_.find_on_right(token);
			if(r == std::string::npos) {
				failed_ +=1;
			} else {
				stack_.emplace_back(state_t { source_.index() } );
				out.put(source_.right_context_to(r));
				source_.move_to(r + token.length());
			}
		} else {
			failed_++;
		}
		return *this;
	}

	parser& parser::skipws(bool require) {
		if(!failed_ && source_.has_next()) {
			source_.mark();
			for(;source_.has_next() && std::isspace(source_.current()); source_.move(1));
			stack_.emplace_back( state_t { source_.get_mark() });
#ifdef PARSER_DEBUG
			std::cout << ">>> skipws from " << start << " to " << source_.index() << std::endl;
#endif
			const std::string result = source_.right_from_mark();
			if(require && result.empty())
				failed_++;
		} else {
			failed_ ++;
		}
		return *this;
	}

	parser& parser::try_comma() {
		if(!failed_ && source_.has_next()) {
			push();
			skipws(false);
			try_token(",");
			skipws(false);
			if(failed_) {
				back(3);
				failed_++;
			}
			compress();
			pop();
		} else {
			failed_++;
		}
		return *this;
	}
	
	parser& parser::try_token_ws(const std::string& token) {
		push();
		try_token(token);
		skipws(true);
		compress();
		pop();
		return *this;
	}

	parser& parser::skip_to_end(token_sink out) {
		if(!failed_) {
			stack_.emplace_back( state_t { source_.index() });
			out.put(source_.right_until_end());
			source_.move_to(source_.length());
		} else {
			failed_++;
		}
		return *this;
	}

	parser& parser::try_parenthesis_expression(token_sink out) {
#ifdef PARSER_DEBUG
		std::cout << ">>>(" << failed_ << ") find parenthesis expression at '" << source_.right_context(20) << "'\n";
#endif
		if(!failed_ && source_.has_next() && source_.current() == '(') {
			push();
			source_.mark();
			int bracket_count = 1;
			bool escaped = false, string = false, string2 = false;
			source_.next();
			for(char c = source_.current(); source_.has_next() && bracket_count > 0; c = source_.next()) {
				if(!string && !string2 && c == '(')
					bracket_count++;
				else if(!string && !string2 && c == ')')
					bracket_count--;
				else if( (string || string2) && escaped && c == '\\')
					escaped = false;
				else if( (string || string2) && !escaped && c == '\\')
					escaped = true;
				else if(!string && !string2 && c == '"') 
					string = true;
				else if(string && !escaped && c == '"')
					string = false;
				else if(!string && !string2 && c == '\'')
					string2 = true;
				else if(string2 && !escaped && c == '\'')
					string2 = false;
				else 
					escaped = false;
#ifdef PARSER_VERY_DEBUG
				std::cout << ">>>> at " << index_ << "=" << c << ": escaped=" << escaped  << ", string = " << string << ", string2 = " << string2 << ", brackets = " << bracket_count << std::endl;
#endif

			}
			if(bracket_count == 0) {
				out.put(source_.right_from_mark());
			} else {
				source_.move_to(source_.get_mark());
				source_.unmark();
				failed_++;
			}
			compress();
			pop();
		} else {
			failed_ ++;
		} 
		return *this;
	}


	void parser::raise(const std::string& msg) {
		const int context = 70;
		const std::string left = source_.left_context(context);
		const std::string right = source_.right_context(context);
		throw std::runtime_error("Parse error at line " + line().filename + ":" + boost::lexical_cast<std::string>(line().line) + ", file offset " +
				boost::lexical_cast<std::string>(source_.index()) + " near '\n\e[1;32m" + left + "\e[1;31m" + right + "\e[0m': " + msg); 		
	}

	void parser::raise_at_line(const file_position_t& file, const std::string& msg) {
		const int context = 70;
		const size_t orig_index = source_.index();
		while(source_.line().line > file.line) // TODO: make a function for it
			source_.move(-1);
		while(source_.line().line < file.line)
			source_.move(1);
		const std::string left = source_.left_context(context);
		const std::string right = source_.right_context(context);		
		source_.move_to(orig_index);
		throw std::runtime_error("Error at file " + file.filename + ":" + boost::lexical_cast<std::string>(file.line) + " near '\e[1;32m" + left + "\e[1;31m" + right + "\e[0m': " + msg); 
	}

	bool parser::failed() const {
		return failed_ != 0;
	}	

	parser::operator bool() const {
		return !failed(); 
	}

	bool parser::finished() const {
		return ( stack_.empty() || stack_.back().index == source_.index() ) && !source_.has_next(); 
	}

	parser& parser::back(size_t n) {
#ifdef PARSER_DEBUG
		std::cout << "back for " << n << " levels, " << failed_ << " failed, stack size is " << stack_.size() << std::endl;
		std::cerr << ">> back " << n << " levels\n";
		std::cerr << "\tfrom " << failed_ << "/:" << source_.right_context(20) << std::endl;
#endif

		if(n > failed_ + stack_.size())
			throw std::logic_error("Attempt to clear more tokens then stack_.size()+failed_");

		if(n >= failed_) {
			n -= failed_;
			failed_ = 0;
		
			while(n-- > 0) {
				source_.move_to(stack_.back().index);
				stack_.pop_back();
#ifdef PARSER_DEBUG
				std::cerr << "\tmoved to " << source_.index() << "/:" << source_.right_context(20) << std::endl;
#endif
			}
		} else {
			failed_ -= n;
		}
#ifdef PARSER_DEBUG
		std::cerr << "\tto " << failed_ << "/:" << source_.right_context(20) << std::endl;
#endif
		return *this;
	}


	void parser::push() {
		state_stack_.push({source_.index(),failed_});
	}

	void parser::compress() {
		if(state_stack_.empty())
			throw std::logic_error("bug: empty stack in compress");
		
#ifdef PARSER_DEBUG
		std::cerr << ">> compress " << stack_.size() << ", " << failed_ << " to ";
#endif
		size_t index, failed;
		std::tie(index, failed) = state_stack_.top();

		while(!stack_.empty() && stack_.back().index > index) {
			stack_.pop_back();
		}
		
		// leave exactly one thing: failed_ OR stack_ element
		if(failed_ > failed && failed_ - failed > 1) {
			failed_ = failed+1;
		} else {
			if(stack_.empty() || stack_.back().index < index) {
				stack_.push_back({index});
			}
		}

#ifdef PARSER_DEBUG
		std::cerr << stack_.size() << "," << failed_ <<  std::endl;
#endif
	}

	void parser::pop() {
		if(state_stack_.empty())
			throw std::logic_error("bug: empty stack in pop");
		state_stack_.pop();
	}

	parser& parser::reset() {
		if(state_stack_.empty())
			throw std::logic_error("Attempt to reset with empty state stack");
		source_.move_to(state_stack_.top().first);
		failed_ = state_stack_.top().second;

		return *this;
	}

	template_parser::template_parser(const std::vector<std::string>& files)
		: p(files) 
		, tree_(std::make_shared<ast::root_t>()) 
		, current_(tree_) {}
		

	ast::root_ptr template_parser::tree() {
		return tree_;
	}

	void template_parser::write(generator::context& context, std::ostream& o) {
		try {
			context.output_mode = tree()->mode();
			if(context.output_mode.empty())
				context.output_mode = "html"; // TODO: context.load_defaults()
			tree()->write(context, o);
		} catch(const cppcms::templates::error_at_line& e) {
			p.raise_at_line(e.line(), e.what());
		}
	}

	void template_parser::parse() {
		try {
			while(!p.finished() && !p.failed()) {
				p.push();
				std::string tmp;
				if(p.reset().skip_to("<%", tmp)) { // [ <html><blah>..., <% ] = 2
#ifdef PARSER_DEBUG
					std::cout << ">>> main -> <%\n";
#endif
					size_t pos = tmp.find("%>");
					if(pos != std::string::npos) {
						p.back(1).skip_to("%>").back(1).raise("unexpected %>");
					}
					add_html(tmp);

					p.push();
					if(p.try_token("=").skipws(false)) { // [ <html><blah>..., <%, =, \s*] = 3
#ifdef PARSER_DEBUG
						std::cout << ">>>\t <%=\n";
#endif
						if(!try_variable_expression()) {
							p.raise("expected variable expression");
						}
					} else if(p.reset().skipws(false)) { // [ <html><blah>..., <%, \s+ ] = 3
						if(!try_flow_expression() && !try_global_expression() && !try_render_expression()) {
							// compat
							if(!try_variable_expression()) {
								p.raise("expected c++, global, render or flow expression or (deprecated) variable expression");
							} else {
								std::cerr << "WARNING: do not use deprecated variable syntax <% var %> at line " << p.line().filename << ":" << p.line().line << std::endl;
							}
						} 
					} else {
						p.raise("expected c++, global, render or flow expression or (deprecated) variable expression");
					}
					p.pop();
				} else if(p.reset().skip_to("%>")) {
					p.reset().raise("found unexpected %>");
				} else if(p.reset().skip_to_end(tmp)) { // [ <blah><blah>EOF ]
#ifdef PARSER_DEBUG
					std::cout << ">>> main -> skip to end\n";
#endif
					add_html(tmp);
				} else {
					p.reset().raise("expected <%=, <% or EOF");
				}
				if(!p) {
					p.raise("syntax error"); // FIXME: make all paths throw its own errors
				}
				p.pop();
			}
		} catch(const error_at_line& e) {
			p.raise_at_line(e.line(), e.what());
		} catch(const bad_cast& e) {
			std::string current_path = (current_->sysname() == "root" ? "skin" : current_->sysname());
			for(auto i = current_->parent(); i && i != i->parent(); i = i->parent()) {
				if(i->sysname() == "root")
					current_path += " / skin";
				else
					current_path += " / " + i->sysname();
			}
			const std::string msg = "\ncurrent object stack: " + current_path 
				+ "\nmaybe you forgot about <% end %>?";

			p.raise(e.what() + msg);	
		} catch(const std::runtime_error& e) {
			p.raise(e.what());
		}
	}

	bool template_parser::try_flow_expression() {
		p.push();
		// ( 'if' | 'elif' ) [ 'not' ] [ 'empty' ] ( VARIABLE | 'rtl' )  
		std::string tmp2;
		if(p.try_one_of_tokens({"if", "elif"}, tmp2).skipws(true)) {
			const std::string verb = tmp2;
			std::string tmp;			
			expr::cpp cond;
			expr::variable variable;
			ast::if_t::type_t type = ast::if_t::type_t::if_regular;
			bool negate = false;			
			if(p.try_token_ws("not")) {
				negate = true;
			} else {
				p.back(1);
			}
			if(p.try_token_ws("empty").try_variable_ws(tmp)) { // [ empty, \s+, VARIABLE, \s+ ]				
				variable = expr::make_variable(tmp);
				type = ast::if_t::type_t::if_empty;
			} else if(p.back(2).try_variable_ws(tmp)) { // [ VAR, \s+ ]
				variable = expr::make_variable(tmp);
				type = ast::if_t::type_t::if_regular;
			} else if(p.back(1).try_token("(") && p.back(1).try_parenthesis_expression(tmp).skipws(false)) { // [ (, \s*, expr, \s* ]
				const std::string parenthesed = tmp;
				cond = expr::make_cpp(parenthesed.substr(1,parenthesed.length()-2));
				type = ast::if_t::type_t::if_cpp;
			} else {
				p.raise("expected [not] [empty] ([variable]|rtl) or ( c++ expr )");
			}
				
			struct next_t {
				ast::if_t::condition_t::next_op_t op;
				bool next_negate = false;
				ast::if_t::type_t next_type = ast::if_t::type_t::if_regular;
				expr::variable next_variable;
			};
			std::vector<next_t> next;

			if(type != ast::if_t::type_t::if_cpp) {
				while(p.skipws(false).try_one_of_tokens({"or", "and"}, tmp)) {
					next_t x;
					if(tmp == "and") {
						x.op = ast::if_t::condition_t::next_op_t::op_and;
					} else {
						x.op = ast::if_t::condition_t::next_op_t::op_or;
					}

					x.next_type = ast::if_t::type_t::if_regular;

					p.skipws(false);

					if(p.try_token_ws("not")) {
						x.next_negate = true;
					} else {
						p.back(1);
					}

					if(p.try_token_ws("empty")) {
						x.next_type = ast::if_t::type_t::if_empty;
					} else {
						p.back(1);
					}

					if(p.try_variable_ws(tmp)) {
						x.next_variable = expr::make_variable(tmp);
						next.push_back(x);
					} else {
						p.raise("expected VARIABLE");
					}				
				}
				p.back(2);
			}
			if(!p.skipws(false).try_close_expression()) {
				p.raise("expected %>");
			}

#ifdef PARSER_TRACE
			std::cout << "flow: " << verb << " type " << type << ", cond = " << cond << ", variable = " << variable << std::endl;
#endif
			if(verb == "if") {
				current_ = current_->as<ast::has_children>().add<ast::if_t>(p.line());
			} else if(verb == "elif") {
				if(current_->sysname() == "condition") {
					current_ = current_->parent();
				} else {
					p.raise("unexpected elif found");
				}
			}
				
			if(type == ast::if_t::type_t::if_cpp) {
				current_ = current_->as<ast::if_t>().add_condition(p.line(), cond, negate);				
			} else if(type == ast::if_t::type_t::if_regular && variable->repr() == "rtl") {
				current_ = current_->as<ast::if_t>().add_condition(p.line(), ast::if_t::type_t::if_rtl, negate);
			} else {
				current_ = current_->as<ast::if_t>().add_condition(p.line(), type, variable, negate);
			}
			for(const next_t& x : next) {
				if(x.next_type == ast::if_t::type_t::if_regular && x.next_variable->repr() == "rtl") {
					current_->as<ast::if_t::condition_t>().add_next(x.op, ast::if_t::type_t::if_rtl, expr::variable(), x.next_negate);
				} else {
					current_->as<ast::if_t::condition_t>().add_next(x.op, x.next_type, x.next_variable, x.next_negate);
				}
			}
		} else if(p.reset().try_token_ws("else").try_close_expression()) {
			if(current_->sysname() == "condition") {
				current_ = current_->parent();
			} else {
				p.raise("unexpected else found");
			}
			current_ = current_->as<ast::if_t>().add_condition(p.line(), ast::if_t::type_t::if_else, false);
#ifdef PARSER_TRACE
			std::cout << "flow: else\n";
#endif
		// 'foreach' NAME ['as' IDENTIFIER ] [ 'rowid' IDENTIFIER [ 'from' NUMBER ] ] [ 'reverse' ] 'in' VARIABLE
		} else if(p.reset().try_token_ws("foreach")) {
			std::string tmp;
			if(!p.try_name_ws(tmp)) {
				p.raise("expected NAME");
			}

			const expr::name item_name = expr::make_name(tmp);
			expr::identifier as;
			expr::name rowid;
			expr::variable variable;
			bool reverse = false;
			int from = 0;

			if(p.try_token_ws("as").try_identifier_ws(tmp)) {
				as = expr::make_identifier(tmp);
			} else {
				p.back(2);
			}
			if(p.try_token_ws("rowid").try_name_ws(tmp)) { // docs say IDENTIFIER, but local variable should be NAME
				rowid = expr::make_name(tmp);
			} else {
				p.back(2);
			}

			if(p.try_token_ws("from").try_number_ws(tmp)) {
				from = boost::lexical_cast<int>(tmp);
			} else {
				p.back(2);
			}

			if(p.try_token_ws("reverse")) {
				reverse = true;
			} else {
				p.back(1);
			}

			if(p.try_token_ws("in").try_variable_ws(tmp).try_close_expression()) {
				variable = expr::make_variable(tmp);
			} else {
				p.raise("expected in VARIABLE %>");
			}

			// save to tree		
			current_ = current_->as<ast::has_children>().add<ast::foreach_t>(p.line(), item_name, as, rowid, from, variable, reverse);
			current_ = current_->as<ast::foreach_t>().prefix(p.line());
#ifdef PARSER_TRACE
			std::cout << "flow: foreach (" << item_name << " in " << variable << "; rowid " << rowid << ", reverse " << reverse << ", as " << as << ", from " << from << "\n";
#endif
		} else if(p.reset().try_token_ws("item").try_close_expression()) {
			// current_ is foreach_t > item_prefix
			current_ = current_->parent()->as<ast::foreach_t>().item(p.line());
#ifdef PARSER_TRACE
			std::cout << "flow: item\n";
#endif
		} else if(p.reset().try_token_ws("empty").try_close_expression()) {
			// current_ is foreach_t > something
			current_ = current_->parent()->as<ast::foreach_t>().empty(p.line());
#ifdef PARSER_TRACE
			std::cout << "flow: empty\n";
#endif
		} else if(p.reset().try_token_ws("separator").try_close_expression()) {
			// current_ is foreach_t > something
			current_ = current_->parent()->as<ast::foreach_t>().separator(p.line());
#ifdef PARSER_TRACE
			std::cout << "flow: separator\n";
#endif
		} else if(p.reset().try_token("end")) {
			std::string what;
			if(!p.skipws(true).try_name(what)) {
				p.back(2);
			}
			if(!p.try_close_expression()) {
				p.raise("expected %> after end " + what);
			}
			
			// action
			current_ = current_->end(what, p.line());
#ifdef PARSER_TRACE
			std::cout << "flow: end " << what << "\n";
#endif

		// 'cache' ( VARIABLE | STRING ) [ 'for' NUMBER ] ['on' 'miss' VARIABLE() ] [ 'no' 'triggers' ] [ 'no' 'recording' ]
		} else if(p.reset().try_token_ws("cache")) {
			std::string tmp;
			expr::ptr name;
			expr::variable miss;
			int _for = -1;
			bool no_triggers = false, no_recording = false;
			if(p.try_variable_ws(tmp)) { 
				name = expr::make_variable(tmp);			
			} else if(p.back(1).try_string_ws(tmp)) {
				name = expr::make_string(tmp);
			} else {
				p.raise("expected VARIABLE or STRING");
			}

			if(p.try_token_ws("for").try_number_ws(tmp)) {
				_for = boost::lexical_cast<int>(tmp);
			} else {
				p.back(2);
			}

			if(p.try_token_ws("on").try_token_ws("miss").try_variable_ws(tmp)) { // TODO: () is required at the end of expression, but try_variable already consumes it
				miss = expr::make_variable(tmp);
			} else {
				p.back(3);
			}

			if(p.try_token_ws("no").try_token_ws("triggers")) {
				no_triggers = true;
			} else {
				p.back(2);
			}

			if(p.try_token_ws("no").try_token_ws("recording")) {
				no_recording = true;
			} else {
				p.back(2);
			}
			
			if(!p.skipws(false).try_close_expression()) {
				p.raise("expected %>");
			}

			current_ = current_->as<ast::has_children>().add<ast::cache_t>(p.line(), name, miss, _for, !no_recording, !no_triggers);
#ifdef PARSER_TRACE
			std::cout << "flow: cache " << name << ", miss = " << miss << ", for " << _for << ", no_triggers = " << no_triggers << ", no_recording = " << no_recording << "\n";
#endif
		} else if(p.reset().try_token_ws("trigger")) {
			std::string tmp;
			expr::ptr name;
			if(p.try_variable(tmp)) {
				name = expr::make_variable(tmp);
			} else if(p.back(1).try_string(tmp)) {
				name = expr::make_string(tmp);
			} else  {
				p.raise("expected STRING or VARIABLE");
			}

			current_ = current_->as<ast::cache_t>().add_trigger(p.line(), name);
#ifdef PARSER_TRACE
			std::cout << "flow: trigger " << name << std::endl;
#endif
			if(!p.skipws(false).try_close_expression()) {
				p.raise("expected %>");
			}
		} else {
			p.reset();
			return false;
		}
		p.pop();
		return true;
	}

	bool template_parser::try_global_expression() {
		p.push();
		std::string tmp2;
		if(p.try_token_ws("skin")) { //  [ skin, \s+ ] 
			std::string skin_name;
			p.push();
			if(p.try_close_expression()) { // [ skin, \s+, %> ]
				skin_name = "__default__";
			} else if(!p.reset().try_name_ws(skin_name).try_close_expression()) { // [ skin, \s+, skinname, \s+, %> ]
				p.reset().raise("expected %> or skin name");
			}
			p.pop();
			
			// save to tree			
			current_ = current_->as<ast::root_t>().add_skin(expr::make_name(skin_name), p.line());
#ifdef PARSER_TRACE
			std::cout << "global: skin " << skin_name << "\n";
#endif
		} else if(p.reset().try_token_ws("view")) { // [ view ]
			p.push();
			std::string view_name, data_name, parent_name;
			if(p.try_name_ws(view_name).try_token_ws("uses").try_identifier_ws(data_name)) { // [ view, NAME, \s+ , uses, \s+, IDENTIFIER, \s+]
				p.push();
				if(!p.try_token_ws("extends").try_name(parent_name)) { // [ view, NAME, \s+, uses, \s+, IDENTIFIER, \s+, extends, \s+ NAME, \s+ ] 
					p.reset();
				}
				p.pop();
			} else {
				p.reset().raise("expected view NAME uses IDENTIFIER [extends NAME]");
			}
			if(p.try_close_expression()) {
				expr::name parent_name_ = (parent_name.empty() ? expr::name() : expr::make_name(parent_name));
				current_ = current_->as<ast::root_t>().add_view(
					expr::make_name(view_name), 
					p.line(),
					expr::make_identifier(data_name), 
					parent_name_);
#ifdef PARSER_TRACE
				std::cout << "global: view " << view_name << "/" << data_name << "/" << parent_name << "\n";
#endif
			} else {
				p.reset().raise("expected %> after view definition");
			}
			p.pop();
		} else if(p.reset().try_token_ws("theme")) {
			p.push();
			std::string view_name, parent_name;
			if(p.try_name_ws(view_name)) { // [ view, NAME, \s+ , uses, \s+, IDENTIFIER, \s+]
				p.push();
				if(!p.try_token_ws("extends").try_name(parent_name)) { // [ view, NAME, \s+, uses, \s+, IDENTIFIER, \s+, extends, \s+ NAME, \s+ ] 
					p.reset();
				}
				p.pop();
			} else {
				p.reset().raise("expected form theme NAME [extends NAME]");
			}
			if(p.try_close_expression()) {
				expr::name parent_name_ = (parent_name.empty() ? expr::name() : expr::make_name(parent_name));
				current_ = current_->as<ast::root_t>().add_form_theme(
					expr::make_name(view_name), 
					p.line(),
					parent_name_);
#ifdef PARSER_TRACE
				std::cout << "global: form theme " << view_name << "/" << data_name << "/" << parent_name << "\n";
#endif
			} else {
				p.reset().raise("expected %> after view definition");
			}
			p.pop();
		} else if(p.reset().try_token_ws("template")) { // [ template, \s+, name, arguments, %> ]			
			std::string function_name, arguments;
			token_sink argsink(arguments);
			if(!p.try_name(function_name).try_param_list(argsink).try_close_expression()) {
				p.raise("expected NAME(params...) %>");
			}

			expr::param_list_t::params_t params;
			std::string name, type, is_const, is_ref;
			while(argsink.has_details()) {
				const auto top = argsink.get_detail();
				if(top.what == "name") {
					name = top.item;
				} else if(top.what == "type") {
					type = top.item;
				} else if(top.what == "is_const") {
					is_const = top.item;
				} else if(top.what == "is_ref") {
					is_ref = top.item;
				} else if(top.what == "param_end") {
					params.push_back({ 
							expr::make_identifier(type),
							(is_const == "const"),
							(is_ref == "ref"),
							expr::make_name(name)
					});
				} else {
					throw std::logic_error("bug: .what == " + top.what);
				}
			}
			
			// save to tree
			current_ = current_->as<ast::view_t>().add_template(
					expr::make_name(function_name),
					p.line(),
					expr::make_param_list(arguments, params));
#ifdef PARSER_TRACE
			std::cout << "global: template " << function_name << "\n";
#endif
		} else if(p.reset().try_token_ws("c++")) { // [ c++, \s+, cppcode, %> ] = 4
			std::string tmp;
			if(!p.skip_to("%>", tmp)) {
				p.raise("expected cppcode %>");
			}
			add_cpp(expr::make_cpp(tmp));
		} else if(p.reset().try_one_of_tokens({"html", "xhtml", "text"}, tmp2).skipws(false).try_close_expression()) {
			const std::string mode = tmp2;

			current_ = current_->as<ast::root_t>().set_mode(mode, p.line());
#ifdef PARSER_TRACE
			std::cout << "global: mode " << mode << std::endl;
#endif
		} else {
			p.reset();
			return false;
		}
		p.pop();
		return true;
	}
		
	ast::using_options_t template_parser::parse_using_options(std::vector<std::string>& variables) {
		// variables is array of 'raw', 'unprocesses' strings after 'using'
		// and result options are processed 
		if(p.try_token_ws("using")) {
			std::string tmp;
			token_sink filter_sink(tmp);
			while(p.skipws(false).try_complex_variable(filter_sink)) { // [ \s*, variable ]					
				variables.emplace_back(tmp);
#ifdef PARSER_TRACE
				std::cout << "\tvariable " << tmp << std::endl;
#endif
				if(!p.skipws(false).try_token(",")) {
					p.back(2);
					break;
				} 
			}
			if(!p) { // found ',', but next complex variable was not found
				p.back(2).raise("expected complex variable");
			}
			
			ast::using_options_t options;
			std::vector<std::string> filters;
			// it needs reversed stack, working on not-reversed stack is difficult
			std::vector<token_sink::detail_t>  rstack;
			while(filter_sink.has_details()) {
				rstack.push_back(filter_sink.get_detail());
			}
			for(auto i = rstack.rbegin(); i != rstack.rend(); ++i) {
				const auto& op = *i;
				if(op.what == "complex_variable_name") {
					std::vector<expr::filter> x;
					for(auto i = filters.rbegin(); i != filters.rend(); ++i) 
						x.emplace_back(expr::make_filter(*i));

					options.emplace_back(ast::using_option_t { 
						expr::make_variable(op.item), p.line(), x, nullptr
					});
					filters.clear();
				} else if(op.what == "complex_variable") {
					filters.emplace_back(op.item);
				} // else discard
			}
			return ast::using_options_t(options.begin(), options.end());
		} else {
			p.back(1);
			return ast::using_options_t();
		}
	}

	bool template_parser::try_render_expression() {
		p.push();
		std::string tmp2;
		if(p.try_one_of_tokens({"gt", "format", "rformat"}, tmp2)) {
			std::string tmp;
			if(!p.skipws(false).try_string(tmp)) {
				p.raise("expected STRING");
			}
			const std::string verb = tmp2;
			const expr::string fmt = expr::make_string(tmp);
			std::vector<std::string> variables;
			p.skipws(false);
			auto options = parse_using_options(variables);
			if(!p.skipws(false).try_close_expression()) {
				p.raise("expected %> after gt expression");
			}

			current_ = current_->as<ast::has_children>().add<ast::fmt_function_t>(verb, p.line(), fmt, options);
#ifdef PARSER_TRACE
			std::cout << "render: gt " << fmt << "\n";
#endif
		} else if(p.reset().try_token_ws("ngt")) { // [ ngt, \s+, STRING, ',', STRING, ',', VARIABLE, \s+ ]
			std::string tmp1, tmp2, tmp3;
			if(!p.try_string(tmp1).try_comma().try_string(tmp2).try_comma().try_variable_ws(tmp3)) {
				p.raise("expected STRING, STRING, VARIABLE");
			}
			const expr::string singular = expr::make_string(tmp1);
			const expr::string plural = expr::make_string(tmp2);
			const expr::variable variable = expr::make_variable(tmp3);
			std::vector<std::string> variables;
			auto options = parse_using_options(variables);
			if(!p.skipws(false).try_close_expression()) {
				p.raise("expected %> after gt expression");
			}

			current_ = current_->as<ast::has_children>().add<ast::ngt_t>(p.line(), singular, plural, variable, options);
#ifdef PARSER_TRACE
			std::cout << "render: ngt " << singular << "/" << plural << "/" << variable << std::endl;
#endif
		} else if(p.reset().try_token_ws("url")) { // [ url, \s+, STRING, \s+ ]
			std::string tmp;
			if(!p.try_string_ws(tmp)) {
				p.raise("expected STRING");
			}
			const expr::string url = expr::make_string(tmp);
			std::vector<std::string> variables;
			auto options = parse_using_options(variables);
			if(!p.skipws(false).try_close_expression()) {
				p.raise("expected %> after gt expression");
			}

			current_ = current_->as<ast::has_children>().add<ast::fmt_function_t>("url", p.line(), url, options);			
#ifdef PARSER_TRACE
			std::cout << "render: url " << url << std::endl;
#endif
		} else if(p.reset().try_token_ws("include")) { // [ include, \s+, identifier ]
			std::string tmp;
			std::string expr;
			expr::call_list id;
			expr::identifier from, _using;
			expr::variable with;
			if(!p.try_identifier(expr)) {
				p.raise("expected IDENTIFIER");
			}

			p.skipws(false);
			std::string alist;
			token_sink alist_sink(alist);
			p.try_argument_list(alist_sink); // [ argument_list ], cant fail

			if(p.skipws(true).try_token_ws("from").try_identifier_ws(tmp)) { // [ \s+, from, \s+, ID, \s+ ]
				from = expr::make_identifier(tmp);
			} else if(p.back(2).try_token_ws("using").try_identifier_ws(tmp)) { // [ \s+, using, \s+, ID, \s+ ]
				_using = expr::make_identifier(tmp);
				if(p.try_token_ws("with").try_variable_ws(tmp)) { // [ with, \s+, VAR, \s+ ]
					with = expr::make_variable(tmp);
				} else {
					p.back(2);
				} 
			} else {
				p.back(3);
			}
			if(!p.skipws(false).try_close_expression()) {
				p.raise("expected %> after gt expression");
			}
			if(from) {
				id = expr::make_call_list(expr + alist, from->repr() + ".");
			} else if(_using) {
				id = expr::make_call_list(expr + alist, "_using.");
			} else {
				id = expr::make_call_list(expr + alist, "");
			}

			current_ = current_->as<ast::has_children>().add<ast::include_t>(id, p.line(), from, _using, with);
#ifdef PARSER_TRACE
			std::cout << "render: include " << id;
			if(!from.empty())
				std::cout << " from " << from;
			if(!_using.empty())
				std::cout << " using " << _using;
			if(!with.empty()) 
				std::cout << " with " << with;
			else
				std::cout << " with (default)";
			std::cout << std::endl;
			std::cout << "\tparameters " << alist << std::endl;
#endif
		} else if(p.reset().try_token_ws("using")) { // 'using' IDENTIFIER  [ 'with' VARIABLE ] as IDENTIFIER  
			std::string tmp;
			expr::identifier id, as;
			expr::variable with;
			if(!p.try_identifier_ws(tmp)) {
				p.raise("expected IDENTIFIER");
			}
			id = expr::make_identifier(tmp);
			if(p.try_token_ws("with").try_variable_ws(tmp)) {
				with = expr::make_variable(tmp);
			} else {
				p.back(2);
			}
			if(p.try_token_ws("as").try_identifier_ws(tmp)) {
				as = expr::make_identifier(tmp);
			} else {
				p.back(2).raise("expected 'as' IDENTIFIER");
			}
			if(!p.skipws(false).try_close_expression()) {
				p.raise("expected %> after gt expression");
			}

			// save to tree
			current_ = current_->as<ast::has_children>().add<ast::using_t>(p.line(), id, with, as);
#ifdef PARSER_TRACE
			std::cout << "render: using " << id << std::endl;
			std::cout << "\twith " << (with.empty() ? "(current)" : with) << std::endl;
			std::cout << "\tas " << as << std::endl;
#endif
		} else if(p.reset().try_token_ws("form")) { // [ form, \s+, NAME, \s+, VAR, \s+, %> ]			
			std::string tmp1, tmp2;
			if(!p.try_name_ws(tmp1).try_variable_ws(tmp2).try_close_expression()) {
				p.raise("expected form STYLE VARIABLE %>");
			} 

			const expr::name name = expr::make_name(tmp1);
			const expr::variable var = expr::make_variable(tmp2);
			if(name->repr() == "end")
				current_ = current_->end("form", p.line());
			else
				current_ = current_->as<ast::has_children>().add<ast::form_t>(name, p.line(), var);
#ifdef PARSER_TRACE
			std::cout << "render: form, name = " << name << ", var = " << var << "\n";
#endif
		} else if(p.reset().try_token_ws("csrf")) {
			std::string tmp;
			expr::name type;
			if(p.try_name_ws(tmp).try_close_expression()) { // [ csrf, \s+, NAME, \s+, %> ]
				type = expr::make_name(tmp);
			} else if(!p.back(2).try_close_expression()) {
				p.raise("expected csrf style(type) or %>");
			}
			// save to tree
			current_ = current_->as<ast::has_children>().add<ast::csrf_t>(p.line(), type);
#ifdef PARSER_TRACE
			std::cout << "render: csrf " << ( type.empty() ? "(default)" : type ) << "\n";
#endif
		// 'render' [ ( VARIABLE | STRING ) , ] ( VARIABLE | STRING ) [ 'with' VARIABLE ] 
		} else if(p.reset().try_token_ws("render")) {
			std::string tmp;
			expr::ptr skin, view;
			expr::variable with;
			if(p.try_variable(tmp)) {
				view = expr::make_variable(tmp);
			} else if(p.back(1).try_string(tmp)) {
				view = expr::make_string(tmp);
			} else {
				p.raise("expected STRING or VARIABLE");
			}
				
			if(p.try_comma().try_variable_ws(tmp)) {
				skin = view;
				view = expr::make_variable(tmp);
			} else if(p.back(1).try_string_ws(tmp)) {
				skin = view;
				view = expr::make_string(tmp);
			} else {
				p.back(2).skipws(false);
			}
			
			if(p.try_token_ws("with").try_variable_ws(tmp)) {
				with = expr::make_variable(tmp);
			} else {
				p.back(2);
			}
			
			if(!p.try_close_expression()) {
				p.raise("expected %>");
			}
			// save to tree
			current_ = current_->as<ast::has_children>().add<ast::render_t>(p.line(), skin, view, with);
#ifdef PARSER_TRACE
			std::cout << "render: render\n\tskin = " << (skin.empty() ? "(default)" : skin ) << "\n\tview = " << view << std::endl;
			if(!with.empty())
				std::cout << "\twith " << with << std::endl;
#endif
		} else {
			p.reset();
			return false;
		}
		p.pop();
		return true;
	}

	bool template_parser::try_variable_expression() {
		p.push();
		token_sink sink;
		if(p.try_complex_variable(sink).skipws(false).try_close_expression()) { // [ variable expression, \s*, %> ]
			const expr::variable expr = expr::make_variable(sink.get_detail().item);			
			std::vector<expr::filter> filters;		
			while(sink.has_details() && sink.top_detail().what == "complex_variable") {
				filters.emplace_back(expr::make_filter(sink.get_detail().item));
			}
			
			current_ = current_->as<ast::has_children>().add<ast::variable_t>(expr, p.line(), filters);
#ifdef PARSER_TRACE
			std::cout << "variable: " << expr << std::endl;
#endif
		} else {
			p.reset();
			return false;
		}
		p.pop();
		return true;
	}

	void template_parser::add_html(const std::string& html) {
		if(html.empty() || (!current_->is_a<ast::has_children>() && is_whitespace_string(html))) {
			// ignore whitespaces between <% %> blocks outside templates
			return;
		}
		
		expr::ptr ptr;
		if(tree_->mode() == "html")
			ptr = expr::make_html(html);
		else if(tree_->mode() == "xhtml")
			ptr = expr::make_xhtml(html);
		else
			ptr = expr::make_text(html);
		try {
			current_->as<ast::has_children>().add<ast::text_t>(ptr, p.line());
		} catch(const std::bad_cast&) {
			std::cerr << "ERROR: html/text can not be added to " << current_->sysname() << " node\n\tvalue = ";
			std::cerr << compress_html(html) << std::endl;
			throw;
		}

#ifdef PARSER_TRACE
		std::cout << "html: " << compress_html(html) << std::endl;
#endif
	}

	void template_parser::add_cpp(const expr::cpp& cpp) {
		if(current_->is_a<ast::root_t>())
			current_ = current_->as<ast::root_t>().add_cpp(cpp, p.line());
		else
			current_ = current_->as<ast::has_children>().add<ast::cppcode_t>(cpp, p.line());
#ifdef PARSER_TRACE
		std::cout << "cpp: " << *cpp << std::endl;
#endif
	}

	namespace expr {
		base_t::base_t(const std::string& value)
			: value_(value) {}

		std::string text_t::repr() const { 
			return "\"" + value_ + "\"";
		}

		std::string text_t::code(generator::context&) const {
			return "\"" + value_ + "\"";
		}

		double number_t::real() const {
			return boost::lexical_cast<double>(value_);
		}

		int number_t::integer() const {
			return boost::lexical_cast<int>(value_);
		}

		std::string number_t::repr() const {
			return value_;
		}
		
		std::string number_t::code(generator::context&) const {
			return value_;
		}

		variable_t::variable_t(const std::string& input, bool consume_all, size_t* pos) 
			: base_t(input) {
			size_t index = 0;
			size_t& i = ( pos == nullptr ? index : *pos );
			
			for(; i < input.length() && std::isspace(input[i]); ++i);

			if(input[i] == '*') {
				is_deref = true;
				++i;
			} else {
				is_deref = false;
			}
			
			for(; i < input.length() && std::isspace(input[i]); ++i);

			std::string name;
			std::vector<ptr> arguments;
			bool function = false;
			for(;i < input.length(); ++i) {
				const char c = input[i];
				if(c == '.') { // separator: .
					parts.push_back({ name, arguments, ".", function });
					arguments.clear();
					function = false;
					name.clear();
				} else if(c == '-' && i < input.length()-1 && input[i+1] == '>') { // sperator: ->
					++i;
					parts.push_back({ name, arguments, "->", function });
					arguments.clear();
					function = false;
					name.clear();
				} else if(c == '(') {
					arguments = parse_arguments(input, i);
					--i;
					function = true;
				} else if(std::isspace(c)) {
					break;
				} else if(c == ',' || c == ')') {
					break;
				} else {
					name += c;
				}
			}

			if(!name.empty()) {
				parts.push_back({ name, arguments, "", function });
			}
			
			for(; i < input.length() && std::isspace(input[i]); ++i);

			if(consume_all && i != input.length()) {
				throw std::runtime_error("Parse error at variable expression, characters left: " + input.substr(i));
			}
		}

		std::vector<ptr> variable_t::parse_arguments(const std::string& input, size_t& i) {
			++i;
			// clear whitespace between '(' and next token
			for(; i < input.length() && std::isspace(input[i]); ++i);
			std::vector<ptr> arguments;
			ptr tmp;
			bool separated = true;
			for(; i < input.length(); ++i) {
				const char c = input[i];
				const bool has_next = (i < input.length()-1);
				const char next = (has_next ? input[i+1] : 0);
				if(separated && c == '"') {
					tmp = parse_string(input, i);
					--i;
					arguments.push_back(tmp);
					separated = false;
				} else if( separated && (
						(c == '-' && has_next && next >= '0' && next <= '9') ||
						(c >= '0' && c <= '9')
						) 
					) {
					tmp = parse_number(input, i);
					--i;
					arguments.push_back(tmp);
					separated = false;
				} else if(std::isspace(c)) {
				} else if(c == ',') {
					separated = true;
				} else if(c == ')') {
					break;
				} else if(separated) {
					tmp = std::make_shared<variable_t>(input, false, &i);
					--i;
					arguments.push_back(tmp);
					separated = false;
				} else {
					throw std::runtime_error("argument is neither string, variable or number: " + input.substr(i));
				}
			}
			if(i < input.length() && input[i] == ')') {
				++i;
				return arguments;
			} else {
				throw std::runtime_error("unterminated argument list");
			}
		}

		ptr variable_t::parse_string(const std::string& input, size_t& i) {
			bool escaped = false;
			size_t start = i;
			++i;
			for(;i < input.length(); ++i) {
				const char c = input[i];

				if(c == '"' && !escaped) {
					break;
				} else if(c == '\\' && !escaped) {
					escaped = true;
				} else {
					escaped = false;
				}
			}
			if(i < input.length() && input[i] == '"') {
				++i;
				return make_string(input.substr(start, i-start));
			} else {
				throw std::runtime_error("unterminated string");
			}
		}

		ptr variable_t::parse_number(const std::string& input, size_t& i) {
			size_t start = i;
			bool oct = false;
			bool hex = false;
			bool dot = false;
			if(input[i] == '-' || input[i] == '+')
				++i;

			if(i < input.length()-2 && input[i] == '0' && input[i+1] == 'x') {
				i += 2;
				hex = true;
			} else if(input[i] == '0') {
				oct = true;
			}

			for(;i < input.length(); ++i) {
				const char c = input[i];
				if( (c >= '0' && c <= '7') ||
					(!oct && c >= '8' && c <= '9') ||
					(hex && c >= 'a' && c <= 'f') ||
					(hex && c >= 'A' && c <= 'F')) {					
				} else if(!dot && c == '.') {
					dot = true;
				} else {
					break;
				}
			}
			return make_number(input.substr(start, i-start));					
		}

		std::string variable_t::repr() const {
			return value_;
		}
		

		const std::pair<std::string, bool> split_exp_filter(const std::string& input) {
			const std::string ext("ext ");
			if(input.compare(0, ext.length(), ext) == 0) {
				return std::make_pair(input.substr(ext.length()), true);
			} else {
				return std::make_pair(input, false);
			}
		}
		filter_t::filter_t(const std::string& input) 
			: call_list_t(split_exp_filter(input).first, 
					split_exp_filter(input).second ? "$var" : "cppcms::filters::")
			, exp_(split_exp_filter(input).second) {}
				
		bool filter_t::is_exp() const { return exp_; }

		std::string filter_t::code(generator::context& context) const {
			if(exp_) {
				return call_list_t::code(context);
			} else {
				return call_list_t::code(context);
			}
		}

		std::string call_list_t::code(generator::context& context) const {
			std::ostringstream oss;
			if(function_prefix_ == "$var")
				oss << context.variable_prefix << value_ << "(  ";
			else
				oss << function_prefix_ << value_ << "(  ";
			if(!current_argument_.empty())
				oss << current_argument_ << ", ";
			for(const ptr& x : arguments_) {
				if(x->is_a<expr::variable_t>())
					oss << x->as<expr::variable_t>().code(context) << ", ";
				else
					oss << x->code(context) << ", ";
			}
			const std::string result = oss.str();
			return result.substr(0, result.length()-2) + ")";
		}

		std::string variable_t::code(generator::context& context) const {
			std::ostringstream o;
			if(is_deref) { 
				o << "*";
			}

			bool first = true;
			for(const part_t& part : parts) {
				if(!first || context.check_scope_variable(part.name)) {
					o << part.name;
				} else {
					o << context.variable_prefix << part.name;
				}

				first = false;

				if(part.is_function) {
					o << "(";
					for(auto i = part.arguments.begin(); i != part.arguments.end(); ++i) {
						if(i != part.arguments.begin())
							o << ", ";
						o << (*i)->code(context);
					}
					o << ")";
				}
				o << part.separator;
			}

			return o.str();
		}

		std::string string_t::repr() const { 
			return value_;
		}
		
		std::string string_t::code(generator::context&) const {
			return value_;
		}

		
		std::string cpp_t::repr() const { 
			return value_;
		}
		
		std::string cpp_t::code(generator::context&) const { 
			return value_;
		}
		

		call_list_t::call_list_t(const std::string& value, const std::string& function_prefix)
			: base_t(split_function_call(value).first)
			, arguments_(split_function_call(value).second) 
			, function_prefix_(function_prefix) {}

		std::string call_list_t::repr() const { 
			std::string result = value_ + "(";
			for(const ptr& x : arguments_)
				result += x->repr() + ",";
			result[result.length()-1] = ')';
			return result;
		}

		call_list_t& call_list_t::argument(const std::string& arg) {
			current_argument_ = arg;
			return *this;
		}
			
		std::string string_t::unescaped() const {
			return decode_escaped_string(value_);
		}
			
		string_t::string_t(const std::string& in) 
			: base_t(compress_string(in)) {}

		std::string name_t::repr() const {
			return value_;
		}
		
		std::string name_t::code(generator::context&) const {
			return value_;
		}
		
		std::string identifier_t::repr() const {
			return value_;
		}
			
		std::string identifier_t::code(generator::context&) const {
			return value_;
		}
		
		param_list_t::param_list_t(const std::string& input, const params_t& params)
			: base_t(trim(input)) 
			, params_(params) {}

		const param_list_t::params_t& param_list_t::params() const { return params_; }

		std::string param_list_t::repr() const {
			return value_;
		}
		
		std::string param_list_t::code(generator::context&) const {
			return value_;
		}

		bool name_t::operator<(const name_t& rhs) const {
			return repr() < rhs.repr();
		}
		
		html make_html(const std::string& repr) {
			return std::make_shared<html_t>(compress_html(repr));
		}
		
		xhtml make_xhtml(const std::string& repr) {
			return std::make_shared<xhtml_t>(compress_html(repr));
		}
		
		text make_text(const std::string& repr) {
			return std::make_shared<text_t>(compress_html(repr));
		}
		
		number make_number(const std::string& repr) {
			return std::make_shared<number_t>(repr);
		}

		variable make_variable(const std::string& repr) {
			return std::make_shared<variable_t>(repr);
		}
		
		filter make_filter(const std::string& repr) {
			return std::make_shared<filter_t>(repr);
		}

		string make_string(const std::string& repr) {
			return std::make_shared<string_t>(repr);
		}

		name make_name(const std::string& repr) {
			return std::make_shared<name_t>(repr);
		}
		
		cpp make_cpp(const std::string& repr) {
			return std::make_shared<cpp_t>(repr);
		}
		
		identifier make_identifier(const std::string& repr) {
			return std::make_shared<identifier_t>(repr);
		}
		
		call_list make_call_list(const std::string& repr, const std::string& prefix) {
			return std::make_shared<call_list_t>(repr, prefix);
		}
		
		param_list make_param_list(const std::string& repr, const param_list_t::params_t& params) {
			return std::make_shared<param_list_t>(repr, params);
		}
		
		std::ostream& operator<<(std::ostream& o, const name_t& obj) {
			return o << "[name:" << obj.repr() << "]";
		}
		
		std::ostream& operator<<(std::ostream& o, const string_t& obj) {
			return o << "[string:" << obj.repr() << "]";
		}
		
		std::ostream& operator<<(std::ostream& o, const identifier_t& obj) {
			return o << "[id:" << obj.repr() << "]";
		}
		
		std::ostream& operator<<(std::ostream& o, const call_list_t& obj) {
			return o << "[calllist:" << obj.repr() << "]";
		}
		
		std::ostream& operator<<(std::ostream& o, const param_list_t& obj) {
			return o << "[paramlist:" << obj.repr() << "]";
		}
		
		std::ostream& operator<<(std::ostream& o, const filter_t& obj) {
			return o << "[filter:" << obj.repr() << "]";
		}
		
		std::ostream& operator<<(std::ostream& o, const variable_t& obj) {
			return o << "[variable:" << obj.repr() << "]";
		}
		
		std::ostream& operator<<(std::ostream& o, const cpp_t& obj) {
			return o << "[cpp:" << obj.repr() << "]";
		}

		std::ostream& operator<<(std::ostream& o, const text_t& obj) {
			return o << "[text:" << obj.repr() << "]";
		}

		std::ostream& operator<<(std::ostream& o, const html_t& obj) {
			return o << "[html:" << obj.repr() << "]";
		}

		std::ostream& operator<<(std::ostream& o, const xhtml_t& obj) {
			return o << "[xhtml:" << obj.repr() << "]";
		}
		
		std::ostream& operator<<(std::ostream& o, const base_t& obj) {
			// currently only usefull types are detected
			o << "[autodetect:";
			if(obj.is_a<number_t>())
				o << obj.as<number_t>();
			else if(obj.is_a<variable_t>())
				o << obj.as<variable_t>();
			else if(obj.is_a<string_t>())
				o << obj.as<string_t>();
			else if(obj.is_a<html_t>())
				o << obj.as<html_t>();
			else if(obj.is_a<text_t>())
				o << obj.as<text_t>();
			else if(obj.is_a<xhtml_t>())
				o << obj.as<xhtml_t>();
			else
				throw std::logic_error(std::string("could not autodetect type ") + typeid(obj).name());
			o << "]";
			return o;
		}
			
	}
	namespace ast {
		base_t::base_t(const std::string& sysname, file_position_t line, bool block, base_ptr parent)
			: sysname_(sysname)
	       		, parent_(parent) 
			, block_(block) 
			, line_(line) {}
		file_position_t base_t::line() const {
			return line_; 
		}	
		base_ptr base_t::parent() { return parent_; }
		
		const std::string& base_t::sysname() const {
			return sysname_;
		}

		bool base_t::block() const { return block_; }

		root_t::root_t() 
			: base_t("root", file_position_t{"__root__", 0}, true, nullptr)		
 			, current_skin(skins.end()) {}

		base_ptr root_t::add_skin(const expr::name& name, file_position_t line) {		
			current_skin = std::find_if(skins.begin(), skins.end(), [=](const skins_t::value_type& skin) {
				return skin.first.repr() == name->repr();
			});
			if(current_skin == skins.end()) {
				skins.emplace_back( *name, skin_t { line, line, view_set_t() } );
				current_skin = --skins.end();
			}
			return shared_from_this();
		}

		base_ptr root_t::set_mode(const std::string& mode, file_position_t line) {
			mode_ = mode;
			mode_line_ = line;
			return shared_from_this();
		}

		base_ptr root_t::add_cpp(const expr::cpp& code, file_position_t line) {
			codes.emplace_back(code_t { line, code });
			return shared_from_this();
		}
		
		base_ptr root_t::add_form_theme(const expr::name& name, file_position_t line, const expr::name& parent) {
			if(current_skin == skins.end())
				throw std::runtime_error("form theme must be inside skin");
		
			auto i = std::find_if(current_skin->second.views.begin(), current_skin->second.views.end(), [=](const view_set_t::value_type& ve) {
				return ve.first.repr() == name->repr();
			});
			if(i == current_skin->second.views.end()) {
				current_skin->second.views.emplace_back(
					*name, std::make_shared<form_theme_t>(name, line, parent, shared_from_this())
				);
				i = --current_skin->second.views.end();
				i->second->as<ast::form_theme_t>().init();
			}
			return i->second;
		}

		base_ptr root_t::add_view(const expr::name& name, file_position_t line, const expr::identifier& data, const expr::name& parent) {
			if(current_skin == skins.end())
				throw std::runtime_error("view must be inside skin");
		
			auto i = std::find_if(current_skin->second.views.begin(), current_skin->second.views.end(), [=](const view_set_t::value_type& ve) {
				return ve.first.repr() == name->repr();
			});
			if(i == current_skin->second.views.end()) {
				current_skin->second.views.emplace_back(
					*name, std::make_shared<view_t>(name, line, data, parent, shared_from_this())
				);
				i = --current_skin->second.views.end();
			}
			return i->second;
		}

		std::string root_t::mode() const { 
			return mode_;
		}

		base_ptr root_t::end(const std::string& what, file_position_t line) {
			const std::string current = (current_skin == skins.end() ? "__root" : "skin");
			if(what.empty() || what == current) {
				current_skin->second.endline = line; 
				current_skin = skins.end();
			} else if(current == "skin") {
				throw std::runtime_error("expected 'end skin', not 'end " + what + "'");
			} else {
				throw std::runtime_error("unexpected 'end '" + what + "'");
			}
			return shared_from_this();
		}

		void root_t::write(generator::context& context, std::ostream& output) {			
			// checks
			// check if there is at least one skin
			if(skins.empty()) {
				throw std::runtime_error("No skins defined");
			}

			// check if there is no conflict between [ -s NAME ] argument and defined skins
			auto i = std::find_if(skins.begin(), skins.end(), [](const skins_t::value_type& skin) {
				return skin.first.repr() == "__default__";
			});

			if(i != skins.end()) {
				if(context.skin.empty()) {
					throw error_at_line("Requested default skin name, but none was provided in arguments", i->second.line);
				} else {
					skins.emplace_back(expr::name_t(context.skin), i->second);
					skins.erase(i);
				}
			}

			std::ostringstream buffer;
			
			for(const code_t& code : codes) {
				buffer << ln(code.line) << code.code->code(context) << std::endl;
			}
			
			for(const skins_t::value_type& skin : skins) {
				if(!context.skin.empty() && context.skin != skin.first.repr())
					throw error_at_line("Mismatched skin names, in argument and template source", skin.second.line);
				buffer << ln(skin.second.line);
				buffer << "namespace " << skin.first.code(context) << " {\n";
				context.current_skin = skin.first.repr();
				for(const view_set_t::value_type& view : skin.second.views) {
					view.second->write(context, buffer);
				}
				buffer << ln(skin.second.endline);
				buffer << "} // end of namespace " << skin.first.code(context) << "\n";
			}
			file_position_t pll = skins.rbegin()->second.endline; // past last line
			pll.line++;

			for(const auto& skinpair : context.skins) {
				buffer << "\n" << ln(pll) << "namespace {\n" << ln(pll) << "cppcms::views::generator my_generator;\n" << ln(pll) << "struct loader {";
				buffer << ln(pll) << "loader() {\n" << ln(pll);			
				buffer << "my_generator.name(\"" << skinpair.first << "\");\n";
				for(const auto& view : skinpair.second.views) {
					buffer << ln(pll) << "my_generator.add_view< " << skinpair.first << "::" << view.name << ", " << view.data << " >(\"" << view.name << "\", true);\n";
				}
				buffer << ln(pll) << "cppcms::views::pool::instance().add(my_generator);\n";
				buffer << ln(pll) << "}\n";
				buffer << ln(pll) << "~loader() { cppcms::views::pool::instance().remove(my_generator); }\n";
				buffer << ln(pll) << "} a_loader;\n";
				buffer << ln(pll) << "} // anon \n";
			}
			
			for(const generator::context::include_t& include : context.includes) {
				output << "#include <" << include << ">\n";
			}
			output << buffer.str();
		}
		
		void view_t::write(generator::context& context, std::ostream& o) {
			context.skins[context.current_skin].views.emplace_back( generator::context::view_t { name_->code(context), data_->code(context) });
			o << ln(line());
			o << "struct " << name_->code(context) << ":public ";
			if(master_)
				o << master_->code(context);
			else
				o << "cppcms::base_view\n";
			o << ln(line()) << " {\n";
			
			o << ln(line());
			o << data_->code(context) << " & content;\n";
			
			o << ln(line());
			o << name_->code(context) << "(std::ostream & _s, " << data_->code(context) << " & _content):";
			if(master_)
				o << master_->code(context) << "(_s, _content)";
			else 
				o << "cppcms::base_view(_s)";
			o << ",content(_content)\n" << ln(line()) << "{\n" << ln(line()) << "}\n";

			for(const templates_t::value_type& tpl : templates) {
				tpl.second->write(context, o);
			}

			o << ln(endline_) << "}; // end of class " << name_->code(context) << "\n";
		}
			
		void root_t::dump(std::ostream& o, int tabs)  const {
			std::string p(tabs, '\t');
			o << p << "root with " << codes.size() << " codes, mode = " << (mode_.empty() ? "(default)" : mode_) << " [\n";
			for(const skins_t::value_type& skin : skins) {
				o << p << "\tskin " << skin.first << " with " << skin.second.views.size() << " views [\n";
				for(const view_set_t::value_type& view : skin.second.views) {
					view.second->dump(o, tabs+2);
				}
				o << p << "\t]\n";
			}
			o << p << "]; codes = [\n";
			for(const code_t& code : codes)
				o << p << "\t" << *code.code << std::endl;
			o << p << "];\n";
		}

		text_t::text_t(const expr::ptr& value, file_position_t line, base_ptr parent)  
			: base_t("text", line, false, parent)
			, value_(value) {}

		void text_t::dump(std::ostream& o, int tabs)  const {
			const std::string p(tabs, '\t');
			o << p << "text: " << *value_ << std::endl;			
		}

		void text_t::write(generator::context& context, std::ostream& o) {
			o << ln(line()) << "out() << " << value_->code(context) << ";\n";
		}
		base_ptr text_t::end(const std::string&, file_position_t) {
			throw std::logic_error("unreachable code -- this is not block node");			
		}


		base_ptr view_t::end(const std::string& what,file_position_t line) {
			if(what.empty() || what == "view") {
				endline_ = line;
				return parent();
			} else {
				throw std::runtime_error("expected 'end view', not 'end " + what + "'");
			}
		}

		void template_t::write(generator::context& context, std::ostream& o) {
			o << ln(line()) << "virtual void " << name_->code(context) << arguments_->code(context) << "{\n";
			for(const auto& param : arguments_->params()) {
				context.add_scope_variable(param.name->code(context));
			}
			for(const base_ptr child : children) {
				child->write(context, o);
			}
			
			for(const auto& param : arguments_->params()) {
				context.remove_scope_variable(param.name->code(context));
			}
			o << ln(endline_) << "} // end of template " << name_->code(context) << "\n";
		}

		base_ptr view_t::add_template(const expr::name& name, file_position_t line, const expr::param_list& arguments) {
			templates.emplace_back(
				*name, std::make_shared<template_t>(name, line, arguments, shared_from_this())
			);
			return templates.back().second; 
		}
		view_t::view_t(const expr::name& name, file_position_t line, const expr::identifier& data, const expr::name& master, base_ptr parent)
			: base_t("view", line, true, parent)
			, name_(name)
			, master_(master) 
			, data_(data)
	       		, endline_(line)	{}
		
		void view_t::dump(std::ostream& o, int tabs)  const {
			const std::string p(tabs, '\t');
			o << p << "view " << *name_ << " uses " << *data_ << " extends ";
			if(master_)
				o << *master_;
			else
				o << "(default)";

			o << " with " << templates.size() << " templates {\n";
			for(const templates_t::value_type& templ : templates) {
				templ.second->dump(o, tabs+1);
			}
			o << p << "}\n";
		}
		
		form_theme_t::form_theme_t(const expr::name& name, file_position_t line, const expr::name& master, base_ptr parent)
			: view_t(name, line, expr::make_identifier("cppcms::themed_form"), master, parent) {}

		void form_theme_t::init() {
			const std::string dispatcher_code = R"c++(
	// FIXME: as i do not see any better way, detect using rtti
	// FIXME: also, i don't have idea for good namespace name for themed<> struct, so use cppcms::widgets::themed<> for now
#define WIDGET(type) \
	if(typeid(widget) == typeid(cppcms::widgets::themed::type)) { \
		this->type(dynamic_cast<cppcms::widgets::themed::type&>(widget)); \
	} else 

	WIDGET(text)
	WIDGET(hidden)
	WIDGET(textarea)
//	WIDGET(numeric)
	WIDGET(password)
	WIDGET(regex_field)
	WIDGET(email)
	WIDGET(checkbox)
	WIDGET(select_multiple)
	WIDGET(select)
	WIDGET(file)
	WIDGET(submit)
	WIDGET(radio)
	else {
		throw std::logic_error("could not detect type of widget: " + std::string(typeid(widget)).name());
	}
#undef WIDGET
			)c++";
			expr::param_list_t::params_t args;
			args.push_back({ expr::make_identifier("cppcms::widgets::base_widget"), false, true, expr::make_name("widget") });
			expr::param_list plist = expr::make_param_list("(cppcms::widgets::base_widget & widget)", args);
			base_ptr dispatcher = add_template(expr::make_name("render_single_widget"), line(), plist);
			dispatcher->as<ast::template_t>().add<ast::cppcode_t>(expr::make_cpp(dispatcher_code), line());
		}

		void form_theme_t::dump(std::ostream& o, int tabs) const {
			o << std::string(tabs, '\t') << "form theme\n";
			view_t::dump(o, tabs+1);
		}
		
		base_ptr form_theme_t::end(const std::string& what,file_position_t line) {
			if(what.empty() || what == "theme") {
				endline_ = line;
				return parent();
			} else {
				throw std::runtime_error("expected 'end theme', not 'end " + what + "'");
			}
		}
			
		has_children::has_children(const std::string& sysname, file_position_t line, bool block, base_ptr parent) 
			: base_t(sysname, line, block, parent)
	       		, endline_(line)	{}
		
		file_position_t has_children::endline() const { return endline_; }
		void has_children::dump(std::ostream& o, int tabs)  const {
			for(const base_ptr& child : children) 
				child->dump(o, tabs);
		}
		
		void has_children::write(generator::context& context, std::ostream& o) {
			for(const base_ptr& child : children) {
				child->write(context, o);
			}
		}

		template_t::template_t(const expr::name& name, file_position_t line, const expr::param_list& arguments, base_ptr parent) 
			: has_children("template", line, true, parent)
	       		, name_(name) 
			, arguments_(arguments) {}

		base_ptr template_t::end(const std::string& what,file_position_t line) {
			if(what.empty() || what == "template") {
				endline_ = line;
				return parent();
			} else {
				throw std::runtime_error("expected 'end template', not 'end " + what + "'");
			}
		}

		void template_t::dump(std::ostream& o, int tabs)  const {
			const std::string p(tabs, '\t');
			o << p << "template " << *name_ << " with arguments " << *arguments_ << " and " << children.size() << " children [\n";
			for(const base_ptr& child : children)
				child->dump(o, tabs+1);
			o << p << "]\n";
		}
		
		cppcode_t::cppcode_t(const expr::cpp& code, file_position_t line, base_ptr parent)
			: base_t("c++", line, false, parent)
			, code_(code) {}

		void cppcode_t::dump(std::ostream& o, int tabs)  const {
			const std::string p(tabs, '\t');
			o << p << "c++: " << code_ << std::endl;
		}

		void cppcode_t::write(generator::context& context, std::ostream& o) {
			o << ln(line()) << code_->code(context) << std::endl;
		}

		base_ptr cppcode_t::end(const std::string&, file_position_t) {
			throw std::logic_error("end in non-block component");
		}
		
		variable_t::variable_t(const expr::variable& name, file_position_t line, const std::vector<expr::filter>& filters, base_ptr parent)
			: base_t("variable", line, false, parent)
			, name_(name)
			, filters_(filters) {}

		void variable_t::dump(std::ostream& o, int tabs)  const {
			const std::string p(tabs, '\t');
			o << p << "variable: " << *name_;
			if(filters_.empty())
				o << " without filters\n";
			else {
				o << " with filters: "; 
				for(const expr::filter& filter : filters_) 
					o << " | " << *filter;
				o << std::endl;
			}
		}
		
		base_ptr variable_t::end(const std::string&, file_position_t) {
			throw std::logic_error("end in non-block component");
		}

		std::string variable_t::code(generator::context& context, const std::string& escaper) const {
			const std::string escape = ( escaper.empty() ? "" : escaper + "(" );
			const std::string close = ( escaper.empty() ? "" : ")" );
			if(filters_.empty()) {				
				return escape + name_->code(context) + close;
			} else {
				std::string current = name_->code(context);
				for(auto i = filters_.rbegin(); i != filters_.rend(); ++i) {
					expr::filter filter = *i;
					current = filter->argument(current).code(context);
				}
				return current;
			}
		}

		void variable_t::write(generator::context& context, std::ostream& o) {
			o << ln(line());
			o << "out() << " << code(context) << ";\n";
		}
			
		fmt_function_t::fmt_function_t(	const std::string& name,
			       			file_position_t line,	
						const expr::string& fmt, 
						const using_options_t& uos, 
						base_ptr parent) 
			: base_t(name, line, false, parent)
			, name_(name)
			, fmt_(fmt)
			, using_options_(uos) {}
		
		void fmt_function_t::dump(std::ostream& o, int tabs)  const {
			const std::string p(tabs, '\t');
			o << p << "fmt function " << name_ << ": " << fmt_->repr() << std::endl;
			if(using_options_.empty()) {
				o << p << "\twithout using\n";
			} else {
				o << p << "\twith using options:\n"; 
				for(const using_option_t& uo : using_options_) {
					uo.dump(o, tabs+2);
				}
			}
		}
		
		base_ptr fmt_function_t::end(const std::string&, file_position_t) {
			throw std::logic_error("end in non-block component");
		}

		void fmt_function_t::write(generator::context& context, std::ostream& o) {						
			o << ln(line());
			std::string function_name;

			if(name_ == "gt") {
				function_name = "cppcms::locale::translate";
			} else if(name_ == "url") {
				o << "content.app().mapper().map(out(), " << fmt_->code(context);
				for(const using_option_t& uo : using_options_) {
					o << ", " << uo.code(context, "cppcms::filters::urlencode");
				}
				o << ");\n";
				return;
			} else if(name_ == "format") {
				context.add_include("boost/format.hpp");
				o << "out() << cppcms::filters::escape("
					<< "(boost::format(" << fmt_->code(context) << ")";
				for(const using_option_t& uo : using_options_) {
					o << "% (" << uo.code(context, "") << ")";					
				}
				o << ").str());";
				return;
			} else if(name_ == "rformat") {
				context.add_include("boost/format.hpp");
				o << "out() << (boost::format(" << fmt_->code(context) << ")";
				for(const using_option_t& uo : using_options_) {
					o << "% (" << uo.code(context, "") << ")";					
				}
				o << ").str();";
				return;
			}
			if(using_options_.empty()) {
				o << "out() << " << function_name << "(" << fmt_->code(context) << ");\n";
			} else {
				o << "out() << cppcms::locale::format(" << function_name << "(" << fmt_->code(context) << ")) ";
				for(const using_option_t& uo : using_options_) {
					o << " % (" << uo.code(context) << ")";
				}
				o << ";\n";
			}
		}
		
		ngt_t::ngt_t(	file_position_t line,
				const expr::string& singular, 
				const expr::string& plural,
				const expr::variable& variable,
				const using_options_t& uos, 
				base_ptr parent)
			: base_t("ngt", line, false, parent)
			, singular_(singular)
			, plural_(plural)
			, variable_(variable) 
			, using_options_(uos) {}
		
		void ngt_t::dump(std::ostream& o, int tabs)  const {
			const std::string p(tabs, '\t');
			o << p << "fmt function ngt: " << *singular_ << "/" << *plural_ << " with variable " << *variable_ <<  std::endl;
			if(using_options_.empty()) {
				o << p << "\twithout using\n";
			} else {
				o << p << "\twith using options:\n"; 
				for(const using_option_t& uo : using_options_) {
					uo.dump(o,tabs+2);
				}
			}
		}
		
		base_ptr ngt_t::end(const std::string&, file_position_t) {
			throw std::logic_error("end in non-block component");
		}

		void ngt_t::write(generator::context& context, std::ostream& o) {
			o << ln(line());
			const std::string function_name = "cppcms::locale::translate";
			
			if(using_options_.empty()) {
				o << "out() << " << function_name << "(" 
					<< singular_->code(context) << ", " 
					<< plural_->code(context) << ", " 
					<< variable_->code(context) << ");\n";
			} else {
				o << "out() << cppcms::locale::format(" << function_name << "(" 
					<< singular_->code(context) << ", " 
					<< plural_->code(context) << ", "
					<< variable_->code(context) << ")) ";
				for(const using_option_t& uo : using_options_) {
					o << " % (" << uo.code(context) << ")";
				}
				o << ";\n";
			}
		}
			
		include_t::include_t(	const expr::call_list& name, file_position_t line, const expr::identifier& from, 
					const expr::identifier& _using, const expr::variable& with, 
					base_ptr parent) 
			: base_t("include", line, false, parent) 
			, name_(name)
			, from_(from)
			, using_(_using) 
			, with_(with) {}
		
		void include_t::dump(std::ostream& o, int tabs)  const {
			const std::string p(tabs, '\t');
			o << p << "include " << *name_;

			if(!from_) {
				if(using_) {
					o << " using " << *using_;
					if(with_)
						o << " with " << *with_;
					else
						o << " with (this content)";
				} else {
					o << " from (self)";
				}
			} else if(from_) {
				o << " from " << *from_;
			} else {
				o << " from (self)";
			}
			o << std::endl;
		}
		
		base_ptr include_t::end(const std::string&, file_position_t) {
			throw std::logic_error("end in non-block component");
		}

		void include_t::write(generator::context& context, std::ostream& o) {
			o << ln(line());
			if(from_) {
				if(!context.check_scope_variable(from_->code(context))) {
					throw error_at_line("No local view variable " + from_->code(context) + " found in context.", line());
				}
				o << name_->code(context) << ";";
			} else if(using_) {
				o << "{\n";
				if(with_) {
					o << ln(line()) << "cppcms::base_content::app_guard _g(" << with_->code(context) << ", content);\n";
				}
				o << ln(line()) << using_->code(context) << " _using(out(), ";
				if(with_) {
					o << with_->code(context);
				} else {
					o << "content";
				} 
				o << ");\n";
				o << ln(line());
				o << name_->code(context) << ";\n";
				o << ln(line()) << "}";
			} else {
				o << name_->code(context) << ";";
			}
			o << "\n";
		}

		form_t::form_t(const expr::name& style, file_position_t line, const expr::variable& name, base_ptr parent)
			: has_children("form", line, ( style && ( style->repr() == "block" || style->repr() == "begin")), parent)
			, style_(style)
			, name_(name) {}
		
		void form_t::dump(std::ostream& o, int tabs)  const {
			const std::string p(tabs, '\t');
			o << p << "form style = " << *style_ << " using variable " << *name_ << std::endl;
		}
		
		base_ptr form_t::end(const std::string& x, file_position_t) {
			if(style_->repr() == "block" || style_->repr() == "begin") {
				if(x.empty() || x == "form") {
					return parent();
				} else {
					throw std::runtime_error("Unexpected 'end " + x + "', expected 'end form'");
				}
			} else {
				throw std::logic_error("end in non-block component");
			}
		}
		
		void form_t::write(generator::context& context, std::ostream& o) {
			const std::string mode = context.output_mode;
			if(style_->repr() == "as_table" || style_->repr() == "as_p" || 
					style_->repr() == "as_ul" || style_->repr() == "as_dl" ||
					style_->repr() == "as_space") {
				o << ln(line()) << "{ ";
				o << "cppcms::form_context _form_context(out(), cppcms::form_flags::as_" << mode << ", cppcms::form_flags::" << style_->code(context) << "); ";
				o << "(" << name_->code(context) << ").render(_form_context); ";
				o << "}\n";
			} else if(style_->repr() == "input") {
				o << ln(line()) << " { ";
				o << "cppcms::form_context _form_context(out(),cppcms::form_flags::as_" << mode << ");\n";
				o << ln(line()) << "_form_context.widget_part(cppcms::form_context::first_part);\n";
				o << ln(line()) << "(" << name_->code(context) << ").render_input(_form_context); ";
				o << ln(line()) << "out() << (" << name_->code(context) << ").attributes_string();\n";
				o << ln(line()) << "_form_context.widget_part(cppcms::form_context::second_part);\n";
				o << ln(line()) << "(" << name_->code(context) << ").render_input(_form_context);\n";
				o << ln(line()) << "}\n";
			} else if(style_->repr() == "begin" || style_->repr() == "block") {
				o << ln(line()) << " { ";
				o << "cppcms::form_context _form_context(out(),cppcms::form_flags::as_" << mode << ");\n";
				o << ln(line()) << "_form_context.widget_part(cppcms::form_context::first_part);\n";
				o << ln(line()) << "(" << name_->code(context) << ").render_input(_form_context); ";
				o << ln(line()) << "}\n";
				for(const base_ptr& child : children) {
					child->write(context, o);
				}
				o << ln(endline_) << " { ";
				o << "cppcms::form_context _form_context(out(),cppcms::form_flags::as_" << mode << ");\n";
				o << ln(endline_) << "_form_context.widget_part(cppcms::form_context::second_part);\n";
				o << ln(endline_) << "(" << name_->code(context) << ").render_input(_form_context);\n";
				o << ln(endline_) << "}\n";
			}	
		}
		
		csrf_t::csrf_t(file_position_t line, const expr::name& style, base_ptr parent)
			: base_t("csrf", line, false, parent)
			, style_(style) {}
		
		void csrf_t::dump(std::ostream& o, int tabs)  const {
			const std::string p(tabs, '\t');
			if(style_)
				o << p << "csrf style = " << *style_ << "\n";
			else
				o << p << "csrf style = (default)\n";
		}
		
		base_ptr csrf_t::end(const std::string&, file_position_t) {
			throw std::logic_error("end in non-block component");
		}
		
		void csrf_t::write(generator::context&, std::ostream& o) {
			if(!style_) {
				o << ln(line()) << "out() << \"<input type=\\x22hidden\\x22 name=\\x22_csrf\\x22 value=\\x22\" << content.app().session().get_csrf_token() << \"\\x22 >\\n\"\n;";
			} else if(style_->repr() == "token") {
				o << ln(line()) << "out() << content.app().session().get_csrf_token();\n";
			} else if(style_->repr() == "script")  {
				std::string jscode = R"javascript(                        out() << "\n"
                                "            <script type='text/javascript'>\n"
                                "            <!--\n"
                                "                {\n"
                                "                    var cppcms_cs = document.cookie.indexOf(\""<< content.app().session().get_csrf_token_cookie_name() <<"=\");\n"
                                "                    if(cppcms_cs != -1) {\n"
                                "                        cppcms_cs += '"<< content.app().session().get_csrf_token_cookie_name() <<"='.length;\n"
                                "                        var cppcms_ce = document.cookie.indexOf(\";\",cppcms_cs);\n"
                                "                        if(cppcms_ce == -1) {\n"
                                "                            cppcms_ce = document.cookie.length;\n"
                                "                        }\n"
                                "                        var cppcms_token = document.cookie.substring(cppcms_cs,cppcms_ce);\n"
                                "                        document.write('<input type=\"hidden\" name=\"_csrf\" value=\"' + cppcms_token + '\" >');\n"
                                "                    }\n"
                                "                }\n"
                                "            -->\n"
                                "            </script>\n"
                                "            ";)javascript";
				o << ln(line()) << jscode << "\n";
			} else if(style_->repr() == "cookie") {
				o << ln(line()) << "out() << content.app().session().get_csrf_token_cookie_name();\n";
			} else {
				throw std::logic_error("Invalid csrf style: " + style_->repr());
			}

		}
		
		render_t::render_t(file_position_t line, const expr::ptr& skin, const expr::ptr& view, const expr::variable& with, base_ptr parent)
			: base_t("render", line, false, parent)
			, skin_(skin)
			, view_(view)
			, with_(with) {}
		
		void render_t::dump(std::ostream& o, int tabs)  const {
			const std::string p(tabs, '\t');
			o << p << "render skin = ";
			if(skin_)
				o << *skin_;
			else 
				o << "(current)";

			o << ", view = " << *view_ << " with " ;
			if(with_)
				o << *with_;
			else 
				o << "(current)";
			o << " content\n";
		}
		
		base_ptr render_t::end(const std::string&, file_position_t) {
			throw std::logic_error("end in non-block component");
		}
		
		void render_t::write(generator::context& context, std::ostream& o) {
			o << ln(line()) << "{\n";
			if(with_) {
				o << ln(line());
				o << "cppcms::base_content::app_guard _g(" << with_->code(context) << ", content);\n";
			}
			o << ln(line()) << "cppcms::views::pool::instance().render(";
			if(skin_) {
				o << skin_->code(context);
			} else {
				o << '"' << context.current_skin << '"';
			}
			o << ", " << view_->code(context) << ", out(), ";
			if(with_)
				o << with_->code(context);
			else
				o << "content";
			o << ");\n";
			o << ln(line()) << "}\n";
		}
			
		using_t::using_t(file_position_t line, const expr::identifier& id, const expr::variable& with, const expr::identifier& as, base_ptr parent)
			: has_children("using", line, true, parent)
			, id_(id)
			, with_(with)
			, as_(as) {}
		
		void using_t::dump(std::ostream& o, int tabs)  const {
			const std::string p(tabs, '\t');
			o << p << "using view type " << *id_ << " as " << *as_ << " with ";
			if(with_)
				o << *with_;
			else
				o << "(current)";
			o << " content [\n";
			for(const base_ptr& child : children)
				child->dump(o, tabs+1);
			o << p << "]\n";
		}
		
		base_ptr using_t::end(const std::string& what,file_position_t line) {
			if(what.empty() || what == "using") {
				endline_ = line;
				return parent();
			} else {
				throw std::runtime_error("expected 'end using', not 'end '" + what + "'");
			}
		}
		
		void using_t::write(generator::context& context, std::ostream& o) {
			o << ln(line()) << "{\n";
			if(with_) {
				o << ln(line()) << "cppcms::base_content::app_guard _g(" << with_->code(context) << ", content);\n";
			}
			o << ln(line()) << id_->code(context) << " " << as_->code(context) << "(out(), ";
			if(with_) {
				o << with_->code(context);
			} else {
				o << "content";
			}
			o << ");\n";
			context.add_scope_variable(as_->code(context));
			for(const base_ptr& child : children) {
				child->write(context, o);
			}
			context.remove_scope_variable(as_->code(context));
			o << ln(endline_) << "}\n";
		}
				
		
		if_t::condition_t::condition_t(file_position_t line, type_t type, const expr::cpp& cond, const expr::variable& variable, bool negate, base_ptr parent)
			: has_children("condition", line, true, parent)
			, type_(type)
			, cond_(cond)
			, variable_(variable) 
			, negate_(negate) {}
			
		if_t::if_t(file_position_t line, base_ptr parent)
			: has_children("if", line, true, parent) {}

		void if_t::condition_t::add_next(const next_op_t& no, const type_t& type, const expr::variable& variable, bool negate) {
			next.push_back({
					std::make_shared<condition_t>(line(), type, expr::cpp(), variable, negate, parent()),
					no });
		}

		base_ptr if_t::add_condition(file_position_t line, type_t type, bool negate) {
			if(!conditions_.empty())
				conditions_.back()->end(std::string(), line);
			conditions_.emplace_back(
				std::make_shared<condition_t>(line, type, expr::cpp(), expr::variable(), negate, shared_from_this())
			);
			return conditions_.back();
		}
			
		base_ptr if_t::add_condition(file_position_t line, const type_t& type, const expr::variable& variable, bool negate) {
			if(!conditions_.empty())
				conditions_.back()->end(std::string(), line);
			conditions_.emplace_back(
				std::make_shared<condition_t>(line, type, expr::cpp(), variable, negate, shared_from_this())
			);
			return conditions_.back();
		}
			
		base_ptr if_t::add_condition(file_position_t line, const expr::cpp& cond, bool negate) {
			if(!conditions_.empty())
				conditions_.back()->end(std::string(), line);
			conditions_.emplace_back(
				std::make_shared<condition_t>(line, type_t::if_cpp, cond, expr::variable(), negate, shared_from_this())
			);
			return conditions_.back();
		}
		
		void if_t::dump(std::ostream& o, int tabs)  const {
			const std::string p(tabs, '\t');
			o << p << "if with " << conditions_.size() << " branches [\n";
			for(const condition_ptr& c : conditions_)
				c->dump(o, tabs+1);
			o << p << "]\n";
		}
		
		base_ptr if_t::end(const std::string&, file_position_t) {
			throw std::logic_error("unreachable code (or rather: bug)");
		}
		
		void if_t::write(generator::context& context, std::ostream& o) {
			auto condition = conditions_.begin();
			(*condition)->write(context, o);

			for(++condition; condition != conditions_.end(); ++condition) {
				if((*condition)->type() == type_t::if_else ) {
					o << " else ";
				} else {
					o << "\n" << ln((*condition)->line()) << "else\n";
				}
				(*condition)->write(context, o);
			}
			if(conditions_.back()->type() == type_t::if_else) {
				o << "\n";
			} else {
				o << " // endif\n";
			}
		}
	
		if_t::type_t if_t::condition_t::type() const { return type_; }

		void if_t::condition_t::dump(std::ostream& o, int tabs)  const {
			const std::string p(tabs, '\t');
			auto printer = [&o,&p](const condition_t& self) {
				const std::string neg = (self.negate_ ? "not " : "");
				switch(self.type_) {
					case type_t::if_regular:
						o << p << neg << "true: " << *self.variable_;
						break;
					case type_t::if_empty:
						o << p << neg << "empty: " << *self.variable_;
						break;
					case type_t::if_rtl:
						o << p << neg << "rtl";
						break;
					case type_t::if_cpp:
						o << p  << neg << "cpp: " << *self.cond_;
						break;
					case type_t::if_else:
						o << p << "else: ";
						break;
				}
			};
			o << "if ";
			printer(*this);
			for(const auto& pair : next) {
				if(pair.second == next_op_t::op_or)
					o << " or ";
				else 
					o << " and ";
				printer(*pair.first);
			}

			o << " [\n";
			for(const base_ptr& bp : children) {
				bp->dump(o, tabs+1);
			}
			o << p << "]\n";
		}
		
		base_ptr if_t::condition_t::end(const std::string& what,file_position_t line) {
			if(what.empty() || what == "if") {
				endline_ = line;
				return parent()->parent(); // this <- if_t <- if_parent, aka end if statement
			} else {
				throw std::runtime_error("expected 'end if', not 'end " + what + "', if started at line " + this->line().filename + ":" + boost::lexical_cast<std::string>(this->line().line));
			}
		}
		
		void if_t::condition_t::write(generator::context& context, std::ostream& o) {			
			if(type_ != type_t::if_else) {
				o << ln(line());
				o << "if(";
			}

			auto printer = [&o,&context](const condition_t& self) {
				if(self.negate_)
					o << "!(";

				switch(self.type_) {
				case type_t::if_regular:
					o << "" << self.variable_->code(context) << "";
					break;

				case type_t::if_empty:
					o << "" << self.variable_->code(context) << ".empty()";
					break;

				case type_t::if_rtl:
					o << "(cppcms::locale::translate(\"LTR\").str(out().getloc()) == \"RTL\")";
					break;
				
				case type_t::if_cpp:
					o << "" << self.cond_->code(context) << "";
					break;

				case type_t::if_else:
					break;				
			
				}

				if(self.negate_)
					o << ")";
			};

			printer(*this);
			for(const auto& pair : next) {
				if(pair.second == next_op_t::op_or) {
					o << " || ";
				} else {
					o << " && ";
				}
				printer(*pair.first);
			}
			
			
			if(type_ != type_t::if_else) {
				o << ") {\n";
			} else {
				o << " {\n";
			}
			for(const base_ptr& bp : children) {
				bp->write(context, o);
			}
			o << ln(endline_) << "} ";
		}
			
		foreach_t::foreach_t(	file_position_t line, 
					const expr::name& name, const expr::identifier& as, 
					const expr::name& rowid, const int from,
					const expr::variable& array, bool reverse, base_ptr parent) 
			: base_t("foreach", line, true, parent) 
			, name_(name)
			, as_(as)
			, rowid_(rowid)
			, from_(from)
			, array_(array) 
			, reverse_(reverse) {}
			
		void foreach_t::dump(std::ostream& o, int tabs)  const {
			const std::string p(tabs, '\t');
			o << p << "foreach " << *name_;
			if(as_)
				o << " (as " << *as_ << ")";
			if(rowid_)
				o << " (and rowid named " << *rowid_ << ")";			
			o << " starting from row " << from_;
			o << " in " << (reverse_ ? "reversed array " : "array ") << *array_ << "{\n";
			if(empty_) {
				o << p << "\tempty = [\n";
				empty_->dump(o, tabs+2);
				o << p << "\t]\n";
			} else { 
				o << p << "\tempty not set\n";
			}

			if(separator_) {
				o << p << "\tseparator = [\n";
				separator_->dump(o, tabs+2);
				o << p << "\t]\n";
			} else {
				o << p << "\tseparator not set\n";
			}

			if(item_) {
				o << p << "\titem = [\n";
				item_->dump(o,tabs+2);
				o << p << "\t]\n";
			} else {
				o << p << "\titem not set\n";
			}
			
			if(item_prefix_) {
				o << p << "\titem prefix = [\n";
				item_prefix_->dump(o,tabs+2);
				o << p << "\t]\n";
			} else {
				o << p << "\titem prefix not set\n";
			}
			
			if(item_suffix_) {
				o << p << "\titem suffix = [\n";
				item_suffix_->dump(o,tabs+2);
				o << p << "\t]\n";
			} else {
				o << p << "\titem suffix not set\n";
			}
			o << p << "}\n";
		}
		
		base_ptr foreach_t::end(const std::string&, file_position_t) {
			throw std::logic_error("unreachable code (or rather: bug)");
		}
		
		void foreach_t::write(generator::context& context, std::ostream& o) {
			const std::string array = "(" + array_->code(context) + ")";
			const std::string item = name_->code(context);
			const std::string rowid = (rowid_ ? rowid_->code(context) : "__rowid");
			const std::string type = (as_ ? as_->code(context) : ("CPPCMS_TYPEOF(" + array + ".begin())"));
			const std::string vtype = (as_ ? ("std::iterator_traits <" + type + " >::value_type") : ("CPPCMS_TYPEOF(*" + item + "_ptr)"));

			o << ln(line());
			o << "if(" << array << ".begin() != " << array << ".end()) {\n";
			if(rowid_) {
				o << ln(line()) << "int " << rowid << " = 1;\n";
			}
			if(item_prefix_)
				item_prefix_->write(context, o);

			o << ln(item_->line());
			o << "for (" <<  type << " "<< item << "_ptr = " << array << ".begin(), " << item << "_ptr_end = " << array << ".end(); " 
				<< item << "_ptr != " << item << "_ptr_end; ++" << item << "_ptr";
			if(rowid_)
				o << ", ++" << rowid << ") {\n";
			else
				o << ") {\n";

			o << ln(item_->line());
			o << vtype <<" & " << item << " = *" << item << "_ptr;\n";
			
			if(rowid_) 
				context.add_scope_variable(rowid);
			context.add_scope_variable(item);
			if(separator_) {
				o << ln(separator_->line());
				o << "if(" << item << "_ptr != " << array << ".begin()) {\n";
				separator_->write(context, o);
				o << ln(separator_->endline()) << "} // end of separator\n";
			}
			item_->write(context, o);			

			if(rowid_) 
				context.remove_scope_variable(rowid);
			context.remove_scope_variable(item);
			o << ln(item_->endline()) << "} // end of item\n";

			if(item_suffix_)
				item_suffix_->write(context, o);

			if(empty_) {
				o << ln(empty_->line());
				o << "} else {\n";
				empty_->write(context, o);
				o << ln(empty_->endline()) << "} // end of empty\n";

			} else {
				if(item_suffix_)
					o << ln(item_suffix_->endline());
				else 
					o << ln(item_->endline());
				o << "}\n";
			}

		}

		foreach_t::part_t::part_t(file_position_t line, const std::string& sysname, bool has_end, base_ptr parent) 
			: has_children(sysname, line, true, parent)
			, has_end_(has_end) {}

		base_ptr foreach_t::part_t::end(const std::string& what, file_position_t line) {
			if(has_end_) { // aka: it is 'item'
				if(what.empty() || what == sysname()) {
					endline_ = line;
					return parent()->as<foreach_t>().suffix(line);
				} else {
					throw std::runtime_error("expected 'end " + sysname() + "', not 'end '" + what + "'");
				}
			} else {				
				if(what.empty() || what == "foreach") {
					if(sysname() == "item_prefix") {
						throw std::runtime_error("foreach without <% item %>");
					} else {
						endline_ = line;
						return parent()->parent(); // this <- foreach_t <- foreach parent
					}
				} else {
					throw std::runtime_error("expected 'end foreach', not 'end '" + what + "'");
				}
			}
		}


		has_children_ptr foreach_t::prefix(file_position_t line) {
			if(!item_prefix_)
				item_prefix_ = std::make_shared<part_t>(line, "item_prefix", false, shared_from_this());
			return item_prefix_;
		}
		
		has_children_ptr foreach_t::suffix(file_position_t line) {
			if(!item_suffix_)
				item_suffix_ = std::make_shared<part_t>(line, "item_suffix", false, shared_from_this());
			return item_suffix_;
		}
		
		has_children_ptr foreach_t::empty(file_position_t line) {
			if(!empty_)
				empty_ = std::make_shared<part_t>(line, "item_empty", false, shared_from_this());
			return empty_;
		}
		has_children_ptr foreach_t::separator(file_position_t line) {
			if(!separator_)
				separator_ = std::make_shared<part_t>(line, "item_separator", false, shared_from_this());
			return separator_;
		}
		has_children_ptr foreach_t::item(file_position_t line) {
			if(!item_)
				item_ = std::make_shared<part_t>(line, "item", true, shared_from_this());
			return item_;
		}
			
		cache_t::cache_t(	file_position_t line, const expr::ptr& name, const expr::variable& miss, 
					int duration, bool recording, bool triggers, base_ptr parent) 
			: has_children("cache", line, true, parent) 
			, name_(name)
			, miss_(miss)
			, duration_(duration)
			, recording_(recording)
			, triggers_(triggers) {}
			
		base_ptr cache_t::add_trigger(file_position_t line, const expr::ptr& t) {
			trigger_list_.emplace_back(trigger_t { line, t });
			return shared_from_this();
		}
		
		void cache_t::dump(std::ostream& o, int tabs)  const {
			const std::string p(tabs, '\t');
			o << p << "cache " << *name_;
			if(duration_ > -1)
				o << " (cached for " << duration_ << "s)";
			if(miss_)
				o << " (call " << *miss_ << " on miss)";
			o << " recording is " << (recording_ ? "ON" : "OFF") << 
				" and triggers are " << (triggers_ ? "ON" : "OFF");
			if(trigger_list_.empty()) {
				o << " - no triggers\n";
			} else { 
				o << " - triggers [\n";
				for(const trigger_t& trigger : trigger_list_)
					o << p << "\t" << *trigger.ptr << std::endl;
				o << p << "]\n";
			}
			o << p << "cache children = [\n";
			for(const base_ptr& child : children) {
				child->dump(o, tabs+1);
			}
			o << p << "]\n";
		}
		
		void cache_t::write(generator::context& context, std::ostream& o) {
			o << ln(line()) << "{\n" << "std::string _cppcms_temp_val;\n";
			o << ln(line()) << "\tif (content.app().cache().fetch_frame(" << name_->code(context) << ", _cppcms_temp_val))\n";
			o << ln(line()) << "\t\tout() << _cppcms_temp_val;\n";
			o << ln(line()) << "\telse {";
			o << ln(line()) << "\t\tcppcms::copy_filter _cppcms_cache_flt(out());\n";
			if(recording_) {
				o << ln(line()) << "\t\tcppcms::triggers_recorder _cppcms_trig_rec(content.app().cache());\n";
			}
			if(miss_) {
				o << ln(line()) << "\t\t" << miss_->code(context) << ";\n";
			}
			has_children::write(context, o);
			o << ln(endline()) << "content.app().cache().store_frame(" << name_->code(context) << ", _cppcms_cache_flt.detach(),";
			if(recording_)
				o << "_cppcms_trig_rec.detach(),";
			else
				o << "std::set <std::string > (),";
			o << duration_ << ", " << (triggers_ ? "false" : "true")  << ");\n";
			o << ln(endline()) << "\t}} // cache\n";

		}
		
		base_ptr cache_t::end(const std::string& what,file_position_t line) {
			if(what.empty() || what == "cache") {
				endline_ = line;
				return parent();
			} else {
				throw std::runtime_error("expected 'end cache', not 'end '" + what + "'");
			}
		}	
	}
}}

#include <iostream>
#include <streambuf>
#include <fstream>

void usage(const std::string& self) {
	std::cerr << self << " [--code(default) | --ast | --parse ] [ -s SKIN NAME ] file1.tmpl file2.tmpl ...\n";
	exit(1);
}

int main(int argc, char **argv) {

	std::vector<std::string> files;
	cppcms::templates::generator::context ctx;
	ctx.level = cppcms::templates::generator::context::compat; // TODO: configurable
	ctx.variable_prefix = "content."; // TODO: load defaults
	enum { code, ast, parse } mode = code;

	for(int i=1;i<argc;++i) {
		const std::string v(argv[i]);
		if(v == "--code") {
			mode = code;
		} else if(v == "--ast") {
			mode = ast;
		} else if(v == "--parse") {
			mode = parse;
		} else if(v == "-s") {
			if(i == argc-1) {
				usage(argv[0]);
			} else {
				ctx.skin = argv[i+1];
				++i;
			}
		} else {
			files.emplace_back(argv[i]);
		}
	}

	if(files.empty())
		usage(argv[0]);
	
	try {
		cppcms::templates::template_parser p(files);
		p.parse();
		if(mode == ast) {
			p.tree()->dump(std::cout);
		} else if(mode == code) {
			p.write(ctx, std::cout);
		} else {
			std::ostringstream oss;
			p.write(ctx, oss);
			std::cout << "parse: ok\n";
		}
	} catch(const std::logic_error& e) {
		std::cerr << "logic error(bug): " << e.what() << std::endl;
		return 2;
	} catch(const std::runtime_error& e) {
		std::cerr << e.what() << std::endl;
		return 3;
	}

	return 0;
}
