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
					case '"':
					case '?':
					case '\\':
						result += current;
						break;
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
		const char hex[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
		for(const char c : input) {
			if(c == '"') {
				result += "\\x22";
			} else if(std::isprint(c)) {
				result += c;
			} else if(translate[static_cast<int>(c)] > 0) {
				result += '\\';
				result += translate[static_cast<unsigned char>(c)];
			} else if(c == '\'' || c == '"' || c == '\\') {
				result += '\\';
				result += c;
			} else {
				result += '\\';
				result += 'x';
				result += hex[static_cast<unsigned char>(c)/16];
				result += hex[static_cast<unsigned char>(c)%16];
			}
		}
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
		return std::string(std::istreambuf_iterator<char>{ifs}, std::istreambuf_iterator<char>{});
	}

	parser_source::parser_source(const std::string& filename)
		: input_(readfile(filename))
       		, index_(0)
		, line_(1) 
		, filename_(filename) {}

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

	file_position_t parser_source::line() const {
		return file_position_t { filename_, line_ };
	}

	file_position_t parser::line() const { return source_.line(); }

	parser::parser(const std::string& input)
		: source_(input)
       		, failed_(0) {}
	
	parser& parser::try_token(const std::string& token) {
#ifdef PARSER_DEBUG
		std::cout << ">>>(" << failed_ << ") find token '" << token << "' at '" << source_.right_context(20) << "'\n";
#endif
		if(!failed_ && source_.compare_head(token)) {
			stack_.emplace_back(state_t { source_.index(), token });
#ifdef PARSER_DEBUG
			std::cout << ">>> token " << stack_.back().token << std::endl;
#endif
			source_.move(token.length());
		} else {
			failed_ ++;
		}
		return *this;
	}

	/* NAME is a sequence of Latin letters, digits and underscore starting with a letter. They represent identifiers and can be defined by regular expression such as: [a-zA-Z][a-zA-Z0-9_]* */
	parser& parser::try_name() {
#ifdef PARSER_DEBUG
		std::cout << ">>>(" << failed_ << ") find name at '" << source_.right_context(20) << "'\n";
#endif
		if(!failed_ && source_.has_next()) {
			size_t start = source_.index();
			char c = source_.current();
			if(is_latin_letter(c) || c == '_') {
				for(;source_.has_next() && ( is_latin_letter(c) || is_digit(c) || c == '_'); c = source_.next());
				stack_.emplace_back(state_t { start, source_.left_context_from(start) });
#ifdef PARSER_DEBUG
				std::cout << ">>> name " << stack_.back().token << std::endl;
#endif
			} else {
				failed_ ++;
			}
		} else {
			failed_++;
		}
		return *this;
	}

	parser& parser::try_string() {
#ifdef PARSER_DEBUG
		std::cout << ">>>(" << failed_ << ") find string at '" << source_.right_context(20) << "'\n";
#endif
		if(!failed_ && source_.has_next()) {
			size_t start = source_.index();
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
					source_.move_to(start);
					raise("expected \", found EOF instead");
				} else {
					source_.next();
					stack_.emplace_back(state_t { start, source_.left_context_from(start) }); 
#ifdef PARSER_DEBUG
					std::cout << ">>> string " << stack_.back().token << std::endl;
#endif
				}
			} else {
#ifdef PARSER_DEBUG
				std::cout << ">>> string failed at " << source_.index() << std::endl;
#endif
				failed_++;
			}
		} else {
			failed_++;
		}
		return *this;
	}

	// NUMBER is a number -- sequence of digits that may start with - and include .. It can be defined by the regular expression: \-?\d+(\.\d*)?
	parser& parser::try_number() {		
#ifdef PARSER_DEBUG
		std::cout << ">>>(" << failed_ << ") find number at '" << source_.right_context(20) << "'\n";
#endif
		if(!failed_ && source_.has_next()) {
			size_t start = source_.index();
			char c = source_.current();

			if(c == '-' || c == '+')
				c = source_.next();
			if(c >= '0' && c <= '9') {
				bool dot = false;
				for(; (c >= '0' && c <= '9') || (!dot && c == '.'); c = source_.next()) {
					if(c == '.')
						dot = true;
				}
				stack_.emplace_back(state_t{ start, source_.left_context_from(start) });				
#ifdef PARSER_DEBUG
				std::cout << ">>> string " << stack_.back().token << std::endl;
#endif
			} else {
				failed_++;
			}
		} else { 
			failed_++;
		}
		return *this;
	}

	// VARIABLE is non-empty sequence of NAMES separated by dot "." or "->" that may optionally end with () or begin with * for 
	// identification of function call result. No blanks are allowed. For example: data->point.x, something.else() *foo.bar.	
	parser& parser::try_variable() {
#ifdef PARSER_DEBUG
		std::cout << ">>>(" << failed_ << ") find var at '" << source_.right_context(20) << "'\n";
#endif
		if(!failed_ && source_.has_next()) {
			auto stack_copy = stack_;
			size_t start = source_.index(), end = source_.index();
			char c = source_.current();
			// parse *name((\.|->)name)
			if(c == '*')
				c = source_.next();
			while(try_name()) {
				end = source_.index();
				if(try_token(".")) {
					end = source_.index();
				} else if(back(1).try_token("->")) {
					end = source_.index();
				} else {
					break;
				}
			}
			back(1); // remove either last failed try_name() or last failed try_token("->")
			
			std::swap(stack_, stack_copy);
			
			if(end != start) {
				if(try_token("()")) {
					end = source_.index();
					back(1); // remove from stack
					source_.move_to(end);
				} else {
					back(1);
				}
				stack_.emplace_back(state_t { start, source_.left_context_from(start) });
#ifdef PARSER_DEBUG
				std::cout << ">>> var " << stack_.back().token << std::endl;
#endif
			} else {
				failed_ ++;
			}

		} else {
			failed_ ++;
		}
		return *this;
	}

	parser& parser::try_complex_variable() {
#ifdef PARSER_DEBUG
		std::cout << ">>>(" << failed_ << ") find cvar at '" << source_.right_context(20) << "'\n";
#endif
		if(!failed_ && source_.has_next()) {
			push();
			size_t start = source_.index(), end = source_.index();
			if(try_variable()) {
				const std::string var = get(-1);
				end = source_.index();
				while(skipws(false).try_token("|").skipws(false).try_filter()) { 
					end = source_.index();
					details_.emplace(detail_t { "complex_variable", get(-1) });
				}
				
				back(4);
				pop();
				
				stack_.emplace_back(state_t {start, source_.slice(start, end) });				
				details_.emplace(detail_t { "complex_variable_name", var });
#ifdef PARSER_DEBUG
				std::cout << ">>> complex " << stack_.back().token << std::endl;
#endif
				source_.move_to(end);
			} 
		} else {
			failed_++;
		}
		return *this;
	}	

	// IDENTIFIER is a sequence of NAME separated by the symbol ::. No blanks are allowed. For example: data::page
	parser& parser::try_identifier() {
#ifdef PARSER_DEBUG
		std::cout << ">>>(" << failed_ << ") find id at '" << source_.right_context(20) << "'\n";
#endif
		if(!failed_ && source_.has_next()) {
			auto stack_copy = stack_;
			size_t start = source_.index();

			while(try_name()) {
				if(!try_token("::")) {
					break;
				}
			}
			back(1); // failed try_name() OR failed token

			std::swap(stack_, stack_copy);
			if(source_.index() != start) {
				stack_.emplace_back(state_t { start, source_.left_context_from(start) });
#ifdef PARSER_DEBUG
				std::cout << ">>> id " << stack_.back().token << std::endl;
#endif
			} else {
				failed_++;
			}
		} else {
			failed_++;
		}
		return *this;
	}
	parser& parser::try_argument_list() {
		if(!failed_ && source_.has_next()) {			
			size_t start = source_.index();
			auto stack_copy = stack_;
			if(try_token("(")) {
				if(!skipws(false).try_token(")")) {
					back(2);
					bool closed = false;
					while(!closed) {
						skipws(false);
						if(try_variable()) {
							details_.emplace(detail_t { "argument_variable", get(-1) });
						} else if(back(1).try_string()) {
							details_.emplace(detail_t { "argument_string", get(-1) });
						} else if(back(1).try_number()) {
							details_.emplace(detail_t { "argument_number", get(-1) });
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
			std::swap(stack_, stack_copy);
			stack_.emplace_back(state_t { start, source_.left_context_from(start) });
		} else {
			failed_++;
		}
		return *this;

	}
	// [ 'ext' ] NAME [ '(' ( VARIABLE | STRING | NUMBER ) [ ',' ( VARIABLE | STRING | NUMBER ) ] ... ]
	parser& parser::try_filter() {
#ifdef PARSER_DEBUG
		std::cout << ">>>(" << failed_ << ") find filter at '" << source_.right_context(20) << "'\n";
#endif
		if(!failed_ && source_.has_next()) {
			size_t start = source_.index();
			if(!try_token_ws("ext")) {
				back(2);
			}
			if(try_name()) {
				try_argument_list();
				stack_.pop_back();
				stack_.emplace_back(state_t { start, source_.left_context_from(start) });
#ifdef PARSER_DEBUG
				std::cout << ">>> filter " << stack_.back().token << std::endl;
#endif
			} else {
				failed_++;
			}
		} else {
			failed_ ++;
		}
		return *this;
	}

	parser& parser::try_name_ws() {
		try_name();
		skipws(true);
		return *this;
	}
	
	parser& parser::try_string_ws() {
		try_string();
		skipws(true);
		return *this;
	}
	
	parser& parser::try_number_ws() {
		try_number();
		skipws(true);
		return *this;
	}

	parser& parser::try_variable_ws() {
		try_variable();
		skipws(true);
		return *this;
	}

	parser& parser::try_identifier_ws() {
		try_identifier();
		skipws(true);
		return *this;
	}

	parser& parser::skip_to(const std::string& token) {
		if(!failed_ && source_.has_next()) {
			size_t r = source_.find_on_right(token);
			if(r == std::string::npos) {
				failed_ +=2;
			} else {
				stack_.emplace_back(state_t { source_.index(), source_.right_context_to(r) } );
				stack_.emplace_back(state_t { r, token });
				source_.move_to(r + token.length());
			}
		} else {
			failed_+=2;
		}
		return *this;
	}

	parser& parser::skipws(bool require) {
		if(!failed_ && source_.has_next()) {
			const size_t start = source_.index();
			for(;source_.has_next() && std::isspace(source_.current()); source_.move(1));
			stack_.emplace_back( state_t { start, source_.left_context_from(start) });
#ifdef PARSER_DEBUG
			std::cout << ">>> skipws from " << start << " to " << source_.index() << std::endl;
#endif
			if(require && source_.index() == start)
				failed_++;
		} else {
			failed_ ++;
		}
		return *this;
	}

	parser& parser::try_comma() {
		if(!failed_ && source_.has_next()) {
			size_t start = source_.index();
			skipws(false);
			try_token(",");
			skipws(false);
			if(failed_) {
				back(3);
				failed_++;
			} else {
				size_t end = source_.index();
				back(3);
				stack_.emplace_back(state_t { start, source_.slice(start, end) });
				source_.move_to(end);
			}
		} else {
			failed_++;
		}
		return *this;
	}
	
	parser& parser::try_token_ws(const std::string& token) {
		try_token(token);
		skipws(true);
		return *this;
	}

	parser& parser::try_token_nl(const std::string& token) {
		try_token(token);
		if(!failed_) {
			if(source_.has_next() && source_.current() == '\n')
				source_.next();
		}
		return *this;
	}

	parser& parser::skip_to_end() {
		if(!failed_) {
			stack_.emplace_back( state_t { source_.index(), source_.right_until_end() });
			source_.move_to(source_.length());
		} else {
			failed_++;
		}
		return *this;
	}

	parser& parser::try_parenthesis_expression() {
#ifdef PARSER_DEBUG
		std::cout << ">>>(" << failed_ << ") find parenthesis expression at '" << source_.right_context(20) << "'\n";
#endif
		if(!failed_ && source_.has_next() && source_.current() == '(') {
			size_t start = source_.index();
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
				stack_.emplace_back(state_t { start, source_.left_context_from(start) });
			} else {
				source_.move_to(start);
				failed_++;
			}
		} else {
			failed_ ++;
		} 
		return *this;
	}


	void parser::raise(const std::string& msg) {
		const int context = 70;
		const std::string left = source_.left_context(context);
		const std::string right = source_.right_context(context);
		throw std::runtime_error("Parse error at position " + boost::lexical_cast<std::string>(source_.index()) + " near '\e[1;32m" + left + "\e[1;31m" + right + "\e[0m': " + msg); 
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
		throw std::runtime_error("Error at position " + boost::lexical_cast<std::string>(source_.index()) + " near '\e[1;32m" + left + "\e[1;31m" + right + "\e[0m': " + msg); 
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

	parser::details_t& parser::details() { return details_; }
	
	parser& parser::back(size_t n) {
#ifdef PARSER_DEBUG
		std::cout << "back for " << n << " levels, " << failed_ << " failed, stack size is " << stack_.size() << std::endl;
#endif
		if(n > failed_ + stack_.size())
			throw std::logic_error("Attempt to clear more tokens then stack_.size()+failed_");

		if(n >= failed_) {
			n -= failed_;
			failed_ = 0;
		
			while(n-- > 0) {
				source_.move_to(stack_.back().index);
				stack_.pop_back();
			}
		} else {
			failed_ -= n;
		}
		return *this;
	}


	void parser::push() {
		state_stack_.push({source_.index(),failed_});
	}

	void parser::pop() {
		if(state_stack_.empty())
			throw std::logic_error("Attempt to pop empty state stack");
		state_stack_.pop();
	}

	parser& parser::reset() {
		if(state_stack_.empty())
			throw std::logic_error("Attempt to reset with empty state stack");
		source_.move_to(state_stack_.top().first);
		failed_ = state_stack_.top().second;

		return *this;
	}

	template_parser::template_parser(const std::string& input)
		: p(input) 
		, tree_(std::make_shared<ast::root_t>()) 
		, current_(tree_) {}
		

	ast::root_ptr template_parser::tree() {
		return tree_;
	}

	void template_parser::write(generator::context& context, std::ostream& o) {
		try {
			tree()->write(context, o);
		} catch(const cppcms::templates::error_at_line& e) {
			p.raise_at_line(e.line(), e.what());
		}
	}

	void template_parser::parse() {
		try {
			while(!p.finished() && !p.failed()) {
				p.push();
				if(p.reset().skip_to("<%")) { // [ <html><blah>..., <% ] = 2
#ifdef PARSER_DEBUG
					std::cout << ">>> main -> <%\n";
#endif
					size_t pos = p.get(-2).find("%>");
					if(pos != std::string::npos) {
						p.back(2).skip_to("%>").back(1).raise("unexpected %>");
					}
					add_html(p.get(-2));

					p.push();
					if(p.try_token("=").skipws(false)) { // [ <html><blah>..., <%, =, \s*] = 3
#ifdef PARSER_DEBUG
						std::cout << ">>>\t <%=\n";
#endif
						if(!try_variable_expression()) {
							p.raise("expected variable expression");
						}
					} else if(p.reset().skipws(true)) { // [ <html><blah>..., <%, \s+ ] = 3
						if(!try_flow_expression() && !try_global_expression() && !try_render_expression()) {
							// compat
							if(!try_variable_expression()) {
								p.raise("expected c++, global, render or flow expression or (deprecated) variable expression");
							}
						} 
					}
					p.pop();
				} else if(p.reset().skip_to("%>")) {
					p.reset().raise("found unexpected %>");
				} else if(p.reset().skip_to_end()) { // [ <blah><blah>EOF ]
#ifdef PARSER_DEBUG
					std::cout << ">>> main -> skip to end\n";
#endif
					add_html(p.get(-1));
				} else {
					p.reset().raise("expected <%=, <% or EOF");
				}
				p.pop();
			}
		} catch(const error_at_line& e) {
			p.raise_at_line(e.line(), e.what());
		} catch(const std::bad_cast&) {
			std::string current_path = current_->sysname();
			for(auto i = current_->parent(); i && i != i->parent(); i = i->parent())
				current_path += " / " + i->sysname();
			p.raise("invalid sequence: " + current_path);			
			// FIXME: more verbose
		}
	}

	bool template_parser::try_flow_expression() {
		p.push();
		// ( 'if' | 'elif' ) [ 'not' ] [ 'empty' ] ( VARIABLE | 'rtl' )  
		if(p.try_token_ws("if") || p.back(2).try_token_ws("elif")) {
			const std::string verb = p.get(-2);
			expr::cpp cond;
			expr::variable variable;
			ast::if_t::type_t type = ast::if_t::if_regular;
			bool negate = false;
			if(p.try_token_ws("not")) {
				negate = true;
			} else {
				p.back(2);
			}
			if(p.try_token_ws("empty").try_variable_ws()) { // [ empty, \s+, VARIABLE, \s+ ]				
				variable = expr::make_variable(p.get(-2));
				type = ast::if_t::type_t::if_empty;
			} else if(p.back(4).try_variable_ws()) { // [ VAR, \s+ ]
				variable = expr::make_variable(p.get(-2));
				type = ast::if_t::type_t::if_regular;
			} else if(p.back(2).try_token("(") && p.back(1).try_parenthesis_expression().skipws(false)) { // [ (, \s*, expr, \s* ]
				cond = expr::make_cpp(p.get(-2));
				type = ast::if_t::if_cpp;
			} else {
				p.raise("expected [not] [empty] ([variable]|rtl) or ( c++ expr )");
			}
			if(!p.skipws(false).try_token("%>")) {
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
		} else if(p.reset().try_token_ws("else").try_token_ws("%>")) {
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
		} else if(p.reset().try_token_ws("foreach").try_name_ws()) {
			const expr::name item_name = expr::make_name(p.get(-2));
			expr::identifier as;
			expr::name rowid;
			expr::variable variable;
			bool reverse = false;
			int from = 0;

			if(p.try_token_ws("as").try_identifier_ws()) {
				as = expr::make_identifier(p.get(-2));
			} else {
				p.back(4);
			}
			if(p.try_token_ws("rowid").try_name_ws()) { // docs say IDENTIFIER, but local variable should be NAME
				rowid = expr::make_name(p.get(-2));
			} else {
				p.back(4);
			}

			if(p.try_token_ws("from").try_number_ws()) {
				from = p.get<int>(-2);
			} else {
				p.back(4);
			}

			if(p.try_token_ws("reverse")) {
				reverse = true;
			} else {
				p.back(2);
			}

			if(p.try_token_ws("in").try_variable_ws().try_token("%>")) {
				variable = expr::make_variable(p.get(-3));
			} else {
				p.raise("expected in VARIABLE %>");
			}

			// save to tree		
			current_ = current_->as<ast::has_children>().add<ast::foreach_t>(p.line(), item_name, as, rowid, from, variable, reverse);
			current_ = current_->as<ast::foreach_t>().prefix(p.line());
#ifdef PARSER_TRACE
			std::cout << "flow: foreach (" << item_name << " in " << variable << "; rowid " << rowid << ", reverse " << reverse << ", as " << as << ", from " << from << "\n";
#endif
		} else if(p.reset().try_token_ws("item").try_token("%>")) {
			// current_ is foreach_t > item_prefix
			current_ = current_->parent()->as<ast::foreach_t>().item(p.line());
#ifdef PARSER_TRACE
			std::cout << "flow: item\n";
#endif
		} else if(p.reset().try_token_ws("empty").try_token("%>")) {
			// current_ is foreach_t > something
			current_ = current_->parent()->as<ast::foreach_t>().empty(p.line());
#ifdef PARSER_TRACE
			std::cout << "flow: empty\n";
#endif
		} else if(p.reset().try_token_ws("separator").try_token("%>")) {
			// current_ is foreach_t > something
			current_ = current_->parent()->as<ast::foreach_t>().separator(p.line());
#ifdef PARSER_TRACE
			std::cout << "flow: separator\n";
#endif
		} else if(p.reset().try_token_ws("end")) {
			std::string what;
			if(p.try_name_ws()) {
				what = p.get(-2);
			} else {
				p.back(2);
			}
			if(!p.try_token("%>")) {
				p.raise("expected %> after end [whatever]");
			}
			
			// action
			current_ = current_->end(what, p.line());
#ifdef PARSER_TRACE
			std::cout << "flow: end " << what << "\n";
#endif

		// 'cache' ( VARIABLE | STRING ) [ 'for' NUMBER ] ['on' 'miss' VARIABLE() ] [ 'no' 'triggers' ] [ 'no' 'recording' ]
		} else if(p.reset().try_token_ws("cache")) {
			expr::ptr name;
			expr::variable miss;
			int _for = -1;
			bool no_triggers = false, no_recording = false;
			if(p.try_variable_ws()) { 
				name = expr::make_variable(p.get(-2));			
			} else if(p.back(2).try_string_ws()) {
				name = expr::make_string(p.get(-2));
			} else {
				p.raise("expected VARIABLE or STRING");
			}

			if(p.try_token_ws("for").try_number_ws()) {
				_for = p.get<int>(-2);
			} else {
				p.back(4);
			}

			if(p.try_token_ws("on").try_token_ws("miss").try_variable()) { // TODO: () is required at the end of expression, but try_variable already consumes it
				miss = expr::make_variable(p.get(-1));
			} else {
				p.back(5);
			}

			if(p.try_token_ws("no").try_token_ws("triggers")) {
				no_triggers = true;
			} else {
				p.back(4);
			}

			if(p.try_token_ws("no").try_token_ws("recording")) {
				no_recording = true;
			} else {
				p.back(4);
			}
			
			if(!p.skipws(false).try_token("%>")) {
				p.raise("expected %>");
			}

			current_ = current_->as<ast::has_children>().add<ast::cache_t>(p.line(), name, miss, _for, !no_recording, !no_triggers);
#ifdef PARSER_TRACE
			std::cout << "flow: cache " << name << ", miss = " << miss << ", for " << _for << ", no_triggers = " << no_triggers << ", no_recording = " << no_recording << "\n";
#endif
		} else if(p.reset().try_token_ws("trigger")) {
			expr::ptr name;
			if(p.try_variable()) {
				name = expr::make_variable(p.get(-1));
			} else if(p.back(1).try_string()) {
				name = expr::make_string(p.get(-1));
			} else  {
				p.raise("expected STRING or VARIABLE");
			}

			current_ = current_->as<ast::cache_t>().add_trigger(p.line(), name);
#ifdef PARSER_TRACE
			std::cout << "flow: trigger " << name << std::endl;
#endif
			if(!p.skipws(false).try_token("%>")) {
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

		if(p.try_token_ws("skin")) { //  [ skin, \s+ ] 
			std::string skin_name;
			p.push();
			if(p.try_token("%>")) { // [ skin, \s+, %> ]
				skin_name = "__default__";
			} else if(p.reset().try_name_ws().try_token("%>")) { // [ skin, \s+, skinname, \s+, %> ]
				skin_name = p.get(-3);
			} else {
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
			if(p.try_name_ws().try_token_ws("uses").try_identifier_ws()) { // [ view, NAME, \s+ , uses, \s+, IDENTIFIER, \s+]
				view_name = p.get(-6);
				data_name = p.get(-2);
				p.push();
				if(p.try_token_ws("extends").try_name_ws()) { // [ view, NAME, \s+, uses, \s+, IDENTIFIER, \s+, extends, \s+ NAME, \s+ ] 
					parent_name = p.get(-2);
				} else {
					p.reset();
				}
				p.pop();
			} else {
				p.reset().raise("expected view NAME uses IDENTIFIER [extends NAME]");
			}
			if(p.try_token("%>")) {
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
		} else if(p.reset().try_token_ws("template").try_name().skip_to("%>")) { // [ template, \s+, name, arguments, %> ]
			const std::string function_name = p.get(-3), arguments = p.get(-2);
			
			// save to tree
			current_ = current_->as<ast::view_t>().add_template(
					expr::make_name(function_name),
					p.line(),
					expr::make_param_list(arguments));
#ifdef PARSER_TRACE
			std::cout << "global: template " << function_name << "\n";
#endif
		} else if(p.reset().try_token_ws("c++").skip_to("%>")) { // [ c++, \s+, cppcode, %> ] = 4
			add_cpp(expr::make_cpp(p.get(-2)));
		} else if(p.reset().try_token_ws("html").try_token("%>") || p.back(3).try_token_ws("xhtml").try_token("%>")) { // [ html|xhtml, \s+, %> ]
			const std::string mode = p.get(-3);

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
			while(p.skipws(false).try_complex_variable()) { // [ \s*, variable ]					
				variables.emplace_back(p.get(-1));
#ifdef PARSER_TRACE
				std::cout << "\tvariable " << p.get(-1) << std::endl;
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
			std::vector<parser::detail_t>  rstack;
			while(!p.details().empty()) {
				rstack.push_back(p.details().top());
				p.details().pop();
			}
			for(auto i = rstack.rbegin(); i != rstack.rend(); ++i) {
				const auto& op = *i;
				if(op.what == "complex_variable_name") {
					std::vector<expr::filter> x;
					for(auto i = filters.rbegin(); i != filters.rend(); ++i) 
						x.emplace_back(expr::make_filter(*i));

					options.emplace_back(ast::using_option_t { 
						expr::make_variable(op.item), x 
					});
					filters.clear();
				} else if(op.what == "complex_variable") {
					filters.emplace_back(op.item);
				} // else discard
			}
			return ast::using_options_t(options.begin(), options.end());
		} else {
			p.back(2);
			return ast::using_options_t();
		}
	}

	bool template_parser::try_render_expression() {
		p.push();
		if(p.try_token_ws("gt").try_string_ws() || p.back(4).try_token_ws("pgt").try_string_ws()) { // [ gt|pgt, \s+, string, \s+ ]
			const std::string verb = p.get(-4);
			const expr::string fmt = expr::make_string(p.get(-2));
			std::vector<std::string> variables;
			auto options = parse_using_options(variables);
			if(!p.skipws(false).try_token("%>")) {
				p.raise("expected %> after gt expression");
			}

			current_ = current_->as<ast::has_children>().add<ast::fmt_function_t>(verb, p.line(), fmt, options);
#ifdef PARSER_TRACE
			std::cout << "render: gt " << fmt << "\n";
#endif
		} else if(p.reset().try_token_ws("ngt").try_string().try_comma().try_string().try_comma().try_variable_ws()) { // [ ngt, \s+, STRING, ',', STRING, ',', VARIABLE, \s+ ]
			const expr::string singular = expr::make_string(p.get(-6));
			const expr::string plural = expr::make_string(p.get(-4));
			const expr::variable variable = expr::make_variable(p.get(-2));
			std::vector<std::string> variables;
			auto options = parse_using_options(variables);
			if(!p.skipws(false).try_token("%>")) {
				p.raise("expected %> after gt expression");
			}

			current_ = current_->as<ast::has_children>().add<ast::ngt_t>(p.line(), singular, plural, variable, options);
#ifdef PARSER_TRACE
			std::cout << "render: ngt " << singular << "/" << plural << "/" << variable << std::endl;
#endif
		} else if(p.reset().try_token_ws("url").try_string_ws()) { // [ url, \s+, STRING, \s+ ]
			const expr::string url = expr::make_string(p.get(-2));
			std::vector<std::string> variables;
			auto options = parse_using_options(variables);
			if(!p.skipws(false).try_token("%>")) {
				p.raise("expected %> after gt expression");
			}

			current_ = current_->as<ast::has_children>().add<ast::fmt_function_t>("url", p.line(), url, options);			
#ifdef PARSER_TRACE
			std::cout << "render: url " << url << std::endl;
#endif
		} else if(p.reset().try_token_ws("include").try_identifier()) { // [ include, \s+, identifier ]
			std::string expr = p.get(-1);
			expr::call_list id;
			expr::identifier from, _using;
			expr::variable with;
			p.skipws(false);
			p.try_argument_list(); // [ argument_list ], cant fail
			id = expr::make_call_list(expr + p.get(-1));
			while(!p.details().empty()) 
				p.details().pop(); // aka p.details().clear();

			const std::string alist = p.get(-1);
			if(p.skipws(true).try_token_ws("from").try_identifier_ws()) { // [ \s+, from, \s+, ID, \s+ ]
				from = expr::make_identifier(p.get(-2));
			} else if(p.back(4).try_token_ws("using").try_identifier_ws()) { // [ \s+, using, \s+, ID, \s+ ]
				_using = expr::make_identifier(p.get(-2));
				if(p.try_token_ws("with").try_variable_ws()) { // [ with, \s+, VAR, \s+ ]
					with = expr::make_variable(p.get(-2));
				} else {
					p.back(4);
				} 
			} else {
				p.back(5);
			}
			if(!p.skipws(false).try_token("%>")) {
				p.raise("expected %> after gt expression");
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
		} else if(p.reset().try_token_ws("using").try_identifier_ws()) { // 'using' IDENTIFIER  [ 'with' VARIABLE ] as IDENTIFIER  
			expr::identifier id, as;
			expr::variable with;
			id = expr::make_identifier(p.get(-2));
			if(p.try_token_ws("with").try_variable_ws()) {
				with = expr::make_variable(p.get(-2));
			} else {
				p.back(4);
			}
			if(p.try_token_ws("as").try_identifier_ws()) {
				as = expr::make_identifier(p.get(-2));
			} else {
				p.back(4).raise("expected 'as' IDENTIFIER");
			}
			if(!p.skipws(false).try_token("%>")) {
				p.raise("expected %> after gt expression");
			}

			// save to tree
			current_ = current_->as<ast::has_children>().add<ast::using_t>(p.line(), id, with, as);
#ifdef PARSER_TRACE
			std::cout << "render: using " << id << std::endl;
			std::cout << "\twith " << (with.empty() ? "(current)" : with) << std::endl;
			std::cout << "\tas " << as << std::endl;
#endif
		} else if(p.reset().try_token_ws("form").try_name_ws().try_variable_ws().try_token("%>")) { // [ form, \s+, NAME, \s+, VAR, \s+, %> ]
			const expr::name name = expr::make_name(p.get(-5));
			const expr::variable var = expr::make_variable(p.get(-3));
			current_ = current_->as<ast::has_children>().add<ast::form_t>(name, p.line(), var);
#ifdef PARSER_TRACE
			std::cout << "render: form, name = " << name << ", var = " << var << "\n";
#endif
		} else if(p.reset().try_token_ws("csrf")) {
			expr::name type;
			if(p.try_name_ws().try_token("%>")) { // [ csrf, \s+, NAME, \s+, %> ]
				type = expr::make_name(p.get(-3));
			} else if(!p.back(3).try_token("%>")) {
				p.raise("expected csrf style(type) or %>");
			}
			// save to tree
			current_ = current_->as<ast::has_children>().add<ast::csrf_t>(p.line(), type);
#ifdef PARSER_TRACE
			std::cout << "render: csrf " << ( type.empty() ? "(default)" : type ) << "\n";
#endif
		// 'render' [ ( VARIABLE | STRING ) , ] ( VARIABLE | STRING ) [ 'with' VARIABLE ] 
		} else if(p.reset().try_token_ws("render")) {
			expr::ptr skin, view;
			expr::variable with;
			if(p.try_variable()) {
				view = expr::make_variable(p.get(-1));
			} else if(p.back(1).try_string()) {
				view = expr::make_string(p.get(-1));
			} else {
				p.raise("expected STRING or VARIABLE");
			}
				
			if(p.try_comma().try_variable_ws()) {
				skin = view;
				view = expr::make_variable(p.get(-2));
			} else if(p.back(2).try_string_ws()) {
				skin = view;
				view = expr::make_string(p.get(-2));
			} else {
				p.back(3).skipws(false);
			}
			
			if(p.try_token_ws("with").try_variable_ws()) {
				with = expr::make_variable(p.get(-2));
			} else {
				p.back(4);
			}
			
			if(!p.try_token("%>")) {
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
		if(p.try_complex_variable().skipws(false).try_token("%>")) { // [ variable expression, \s*, %> ]
			const expr::variable expr = expr::make_variable(p.details().top().item);
			std::vector<expr::filter> filters;
			p.details().pop();
			while(!p.details().empty() && p.details().top().what == "complex_variable") {
				filters.emplace_back(expr::make_filter(p.details().top().item));
				p.details().pop();
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

		double number_t::real() const {
			return boost::lexical_cast<double>(value_);
		}

		int number_t::integer() const {
			return boost::lexical_cast<int>(value_);
		}

		std::string number_t::repr() const {
			return value_;
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
			: call_list_t(split_exp_filter(input).first)
			, exp_(split_exp_filter(input).second) {}
				
		bool filter_t::is_exp() const { return exp_; }
			
		std::string filter_t::code(const std::string& function_prefix, const std::string& content_prefix, const std::string argument) const {
			if(exp_) {
				return call_list_t::code(content_prefix, argument);
			} else {
				return call_list_t::code(function_prefix, argument);
			}
		}

		std::string call_list_t::code(const std::string& function_prefix, const std::string argument) const {
			std::ostringstream oss;
			oss << function_prefix << value_ << "(";
			oss << argument;
			for(const ptr& x : arguments_)
				oss << ", " << x->repr();
			oss << ")";
			return oss.str();
		}

		std::string variable_t::prefixed(const std::string& prefix) const {
			if(value_[0] == '*')
				return "*" + prefix + value_.substr(1);
			else
				return prefix + value_;
		}

		std::string string_t::repr() const { 
			return value_;
		}
		
		std::string cpp_t::repr() const { 
			return value_;
		}
		

		call_list_t::call_list_t(const std::string& value)
			: base_t(split_function_call(value).first)
			, arguments_(split_function_call(value).second) {}

		std::string call_list_t::repr() const { 
			std::string result = value_ + "(";
			for(const ptr& x : arguments_)
				result += x->repr() + ",";
			result[result.length()-1] = ')';
			return result;
		}
			
		std::string string_t::unescaped() const {
			return decode_escaped_string(value_);
		}
		std::string name_t::repr() const {
			return value_;
		}
		
		std::string identifier_t::repr() const {
			return value_;
		}
		
		param_list_t::param_list_t(const std::string& input)
			: base_t(trim(input)) {}

		std::string param_list_t::repr() const {
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
		
		call_list make_call_list(const std::string& repr) {
			return std::make_shared<call_list_t>(repr);
		}
		
		param_list make_param_list(const std::string& repr) {
			return std::make_shared<param_list_t>(repr);
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
			current_skin = skins.emplace( *name, skin_t { line, line, view_set_t() } ).first;
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
			
		base_ptr root_t::add_view(const expr::name& name, file_position_t line, const expr::identifier& data, const expr::name& parent) {
			if(current_skin == skins.end())
				throw std::runtime_error("view must be inside skin");
			
			return current_skin->second.views.emplace(
				*name, std::make_shared<view_t>(name, line, data, parent, shared_from_this())
			).first->second;
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

		void root_t::write(generator::context& context, std::ostream& o) {
			for(const code_t& code : codes) {
				o << ln(code.line) << code.code << std::endl;
			}
			auto i = skins.find(expr::name_t("__default__"));
			if(i != skins.end()) {
				if(context.skin.empty()) {
					throw error_at_line("Requested default skin name, but none was provided in arguments", i->second.line);
				} else {
					skins[expr::name_t(context.skin)] = i->second;
				}
				skins.erase(i);
			}
			for(const skins_t::value_type& skin : skins) {
				if(!context.skin.empty() && context.skin != skin.first.repr())
					throw error_at_line("Mismatched skin names, in argument and template source", skin.second.line);
				o << ln(skin.second.line);
				o << "namespace " << skin.first.repr() << " {\n";
				context.current_skin = skin.first.repr();
				for(const view_set_t::value_type& view : skin.second.views) {
					view.second->write(context, o);
				}
				o << ln(skin.second.endline);
				o << "} // end of namespace " << skin.first.repr() << "\n";
			}
			file_position_t pll = skins.rend()->second.endline; // past last line
			pll.line++;

			for(const auto& skinpair : context.skins) {
				o << "\n" << ln(pll) << "namespace {\n" << ln(pll) << "cppcms::views::generator my_generator;\n" << ln(pll) << "struct loader {";
				o << ln(pll) << "loader() {\n" << ln(pll);			
				o << "my_generator.name(\"" << skinpair.first << "\");\n";
				for(const auto& view : skinpair.second.views) {
					o << ln(pll) << "my_generator.add_view< " << skinpair.first << "::" << view.name << ", " << view.data << " >(\"" << view.name << "\", true);\n";
				}
				o << ln(pll) << "cppcms::views::pool::instance().add(my_generator);\n";
				o << ln(pll) << "}\n";
				o << ln(pll) << "~loader() { cppcms::views::pool::instance().remove(my_generator); }\n";
				o << ln(pll) << "} a_loader;\n";
				o << ln(pll) << "} // anon \n";
			}
		}
		
		void view_t::write(generator::context& context, std::ostream& o) {
			context.skins[context.current_skin].views.emplace_back( generator::context::view_t { name_->repr(), data_->repr() });
			o << ln(line());
			o << "struct " << name_->repr() << ":public ";
			if(master_)
				o << master_->repr();
			else
				o << "cppcms::base_view\n";
			o << ln(line()) << " {\n";
			
			o << ln(line());
			o << data_->repr() << " & content;\n";
			
			o << ln(line());
			o << name_->repr() << "(std::ostream & _s, " << data_->repr() << " & _content):";
			if(master_)
				o << master_->repr() << "(_s, _content)";
			else 
				o << "cppcms::base_view(_s)";
			o << ",content(_content)\n" << ln(line()) << "{\n" << ln(line()) << "}\n";

			for(const templates_t::value_type& tpl : templates) {
				tpl.second->write(context, o);
			}

			o << ln(endline_) << "}; // end of class " << name_->repr() << "\n";
		}
			
		void root_t::dump(std::ostream& o, int tabs) {
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

		void text_t::dump(std::ostream& o, int tabs) {
			const std::string p(tabs, '\t');
			o << p << "text: " << *value_ << std::endl;			
		}

		void text_t::write(generator::context& context, std::ostream& o) {
			o << ln(line()) << "out() << " << value_->repr() << ";\n";
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
			o << ln(line()) << "virtual void " << name_->repr() << arguments_->repr() << "{\n";
			for(const base_ptr child : children) {
				child->write(context, o);
			}
			o << ln(endline_) << "} // end of template " << name_->repr() << "\n";
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
		
		void view_t::dump(std::ostream& o, int tabs) {
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
			
		has_children::has_children(const std::string& sysname, file_position_t line, bool block, base_ptr parent) 
			: base_t(sysname, line, block, parent)
	       		, endline_(line)	{}
			
		void has_children::dump(std::ostream& o, int tabs) {
			for(const base_ptr& child : children) 
				child->dump(o, tabs);
		}
		
		void has_children::write(generator::context& context, std::ostream& /* o */) {}

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

		void template_t::dump(std::ostream& o, int tabs) {
			const std::string p(tabs, '\t');
			o << p << "template " << *name_ << " with arguments " << *arguments_ << " and " << children.size() << " children [\n";
			for(const base_ptr& child : children)
				child->dump(o, tabs+1);
			o << p << "]\n";
		}
		
		cppcode_t::cppcode_t(const expr::cpp& code, file_position_t line, base_ptr parent)
			: base_t("c++", line, false, parent)
			, code_(code) {}

		void cppcode_t::dump(std::ostream& o, int tabs) {
			const std::string p(tabs, '\t');
			o << p << "c++: " << code_ << std::endl;
		}

		void cppcode_t::write(generator::context& context, std::ostream& /* o */) {
		}

		base_ptr cppcode_t::end(const std::string&, file_position_t) {
			throw std::logic_error("end in non-block component");
		}
		
		variable_t::variable_t(const expr::variable& name, file_position_t line, const std::vector<expr::filter>& filters, base_ptr parent)
			: base_t("variable", line, false, parent)
			, name_(name)
			, filters_(filters) {}

		void variable_t::dump(std::ostream& o, int tabs) {
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

		void variable_t::write(generator::context& context, std::ostream& o) {
			const std::string escape = "cppcms::filters::escape(";
			const std::string prefix = "content.";
			const std::string filter_prefix = "cppcms::filters::";

			o << ln(line());
			if(filters_.empty()) {				
				o << "out() << " << escape << name_->prefixed(prefix) << ");\n";
			} else {
				std::string current = name_->prefixed(prefix);
				for(auto i = filters_.rbegin(); i != filters_.rend(); ++i) {
					expr::filter filter = *i;
					current = filter->code(filter_prefix, prefix, current);
				}
				o << "out() << " << current << ";\n";
			}
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
		
		void fmt_function_t::dump(std::ostream& o, int tabs) {
			const std::string p(tabs, '\t');
			o << p << "fmt function " << name_ << ": " << fmt_->repr() << std::endl;
			if(using_options_.empty()) {
				o << p << "\twithout using\n";
			} else {
				o << p << "\twith using options:\n"; 
				for(const using_option_t& uo : using_options_) {
					o << p << "\t\t" << *uo.variable;
					if(!uo.filters.empty()) {
						o << " with filters ";
						for(const expr::filter& filter : uo.filters)  {
							o << " | " << *filter;
						}
					}
					o << std::endl;
				}
			}
		}
		
		base_ptr fmt_function_t::end(const std::string&, file_position_t) {
			throw std::logic_error("end in non-block component");
		}

		void fmt_function_t::write(generator::context& context, std::ostream& /* o */) {
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
		
		void ngt_t::dump(std::ostream& o, int tabs) {
			const std::string p(tabs, '\t');
			o << p << "fmt function ngt: " << *singular_ << "/" << *plural_ << " with variable " << *variable_ <<  std::endl;
			if(using_options_.empty()) {
				o << p << "\twithout using\n";
			} else {
				o << p << "\twith using options:\n"; 
				for(const using_option_t& uo : using_options_) {
					o << p << "\t\t" << *uo.variable;
					if(!uo.filters.empty()) {
						o << " with filters ";
						for(const expr::filter& filter : uo.filters)  {
							o << " | " << *filter;
						}
					}
					o << std::endl;
				}
			}
		}
		
		base_ptr ngt_t::end(const std::string&, file_position_t) {
			throw std::logic_error("end in non-block component");
		}

		void ngt_t::write(generator::context& context, std::ostream& /* o */) {
		}
			
		include_t::include_t(	const expr::call_list& name, file_position_t line, const expr::identifier& from, 
					const expr::identifier& _using, const expr::variable& with, 
					base_ptr parent) 
			: base_t("include", line, false, parent) 
			, name_(name)
			, from_(from)
			, using_(_using) 
			, with_(with) {}
		
		void include_t::dump(std::ostream& o, int tabs) {
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

		void include_t::write(generator::context& context, std::ostream& /* o */) {
		}

		form_t::form_t(const expr::name& style, file_position_t line, const expr::variable& name, base_ptr parent)
			: base_t("form", line, false, parent)
			, style_(style)
			, name_(name) {}
		
		void form_t::dump(std::ostream& o, int tabs) {
			const std::string p(tabs, '\t');
			o << p << "form style = " << *style_ << " using variable " << *name_ << std::endl;
		}
		
		base_ptr form_t::end(const std::string&, file_position_t) {
			throw std::logic_error("end in non-block component");
		}
		
		void form_t::write(generator::context& context, std::ostream& /* o */) {
		}
		
		csrf_t::csrf_t(file_position_t line, const expr::name& style, base_ptr parent)
			: base_t("csrf", line, false, parent)
			, style_(style) {}
		
		void csrf_t::dump(std::ostream& o, int tabs) {
			const std::string p(tabs, '\t');
			if(style_)
				o << p << "csrf style = " << *style_ << "\n";
			else
				o << p << "csrf style = (default)\n";
		}
		
		base_ptr csrf_t::end(const std::string&, file_position_t) {
			throw std::logic_error("end in non-block component");
		}
		
		void csrf_t::write(generator::context& context, std::ostream& /* o */) {
		}
		
		render_t::render_t(file_position_t line, const expr::ptr& skin, const expr::ptr& view, const expr::variable& with, base_ptr parent)
			: base_t("render", line, false, parent)
			, skin_(skin)
			, view_(view)
			, with_(with) {}
		
		void render_t::dump(std::ostream& o, int tabs) {
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
		
		void render_t::write(generator::context& context, std::ostream& /* o */) {
		}
			
		using_t::using_t(file_position_t line, const expr::identifier& id, const expr::variable& with, const expr::identifier& as, base_ptr parent)
			: has_children("using", line, true, parent)
			, id_(id)
			, with_(with)
			, as_(as) {}
		
		void using_t::dump(std::ostream& o, int tabs) {
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
		
		void using_t::write(generator::context& context, std::ostream& /* o */) {
		}
				
		
		if_t::condition_t::condition_t(file_position_t line, type_t type, const expr::cpp& cond, const expr::variable& variable, bool negate, base_ptr parent)
			: has_children("condition", line, true, parent)
			, type_(type)
			, cond_(cond)
			, variable_(variable) 
			, negate_(negate) {}
			
		if_t::if_t(file_position_t line, base_ptr parent)
			: has_children("if", line, true, parent) {}

		base_ptr if_t::add_condition(file_position_t line, type_t type, bool negate) {
			conditions_.emplace_back(
				std::make_shared<condition_t>(line, type, expr::cpp(), expr::variable(), negate, shared_from_this())
			);
			return conditions_.back();
		}
			
		base_ptr if_t::add_condition(file_position_t line, const type_t& type, const expr::variable& variable, bool negate) {
			conditions_.emplace_back(
				std::make_shared<condition_t>(line, type, expr::cpp(), variable, negate, shared_from_this())
			);
			return conditions_.back();
		}
			
		base_ptr if_t::add_condition(file_position_t line, const expr::cpp& cond, bool negate) {
			conditions_.emplace_back(
				std::make_shared<condition_t>(line, type_t::if_cpp, cond, expr::variable(), negate, shared_from_this())
			);
			return conditions_.back();
		}
		
		void if_t::dump(std::ostream& o, int tabs) {
			const std::string p(tabs, '\t');
			o << p << "if with " << conditions_.size() << " branches [\n";
			for(const condition_ptr& c : conditions_)
				c->dump(o, tabs+1);
			o << p << "]\n";
		}
		
		base_ptr if_t::end(const std::string&, file_position_t) {
			throw std::logic_error("unreachable code (or rather: bug)");
		}
		
		void if_t::write(generator::context& context, std::ostream& /* o */) {
		}
		
		void if_t::condition_t::dump(std::ostream& o, int tabs) {
			const std::string p(tabs, '\t');
			const std::string neg = (negate_ ? "not " : "");
			switch(type_) {
				case if_regular:
					o << p << "if " << neg << "true: " << *variable_;
					break;
				case if_empty:
					o << p << "if " << neg << "empty: " << *variable_;
					break;
				case if_rtl:
					o << p << "if " << neg << "rtl";
					break;
				case if_cpp:
					o << p << "if " << neg << "cpp: " << *cond_;
					break;
				case if_else:
					o << p << "if else: ";
					break;
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
				throw std::runtime_error("expected 'end if', not 'end '" + what + "'");
			}
		}
		
		void if_t::condition_t::write(generator::context& context, std::ostream& /* o */) {
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
			
		void foreach_t::dump(std::ostream& o, int tabs) {
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
		
		void foreach_t::write(generator::context& context, std::ostream& /* o */) {}

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
					endline_ = line;
					return parent()->parent(); // this <- foreach_t <- foreach parent
				} else {
					throw std::runtime_error("expected 'end foreach', not 'end '" + what + "'");
				}
			}
		}


		base_ptr foreach_t::prefix(file_position_t line) {
			if(!item_prefix_)
				item_prefix_ = std::make_shared<part_t>(line, "item_prefix", false, shared_from_this());
			return item_prefix_;
		}
		
		base_ptr foreach_t::suffix(file_position_t line) {
			if(!item_suffix_)
				item_suffix_ = std::make_shared<part_t>(line, "item_suffix", false, shared_from_this());
			return item_suffix_;
		}
		
		base_ptr foreach_t::empty(file_position_t line) {
			if(!empty_)
				empty_ = std::make_shared<part_t>(line, "item_empty", false, shared_from_this());
			return empty_;
		}
		base_ptr foreach_t::separator(file_position_t line) {
			if(!separator_)
				separator_ = std::make_shared<part_t>(line, "item_separator", false, shared_from_this());
			return separator_;
		}
		base_ptr foreach_t::item(file_position_t line) {
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
		
		void cache_t::dump(std::ostream& o, int tabs) {
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
		
		void cache_t::write(generator::context& context, std::ostream& /* o */) {
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

int main(int argc, char **argv) {
	cppcms::templates::template_parser p(argv[1]);
	p.parse();
	if(std::string(argv[2]) == "--ast")
		p.tree()->dump(std::cout);
	else {
		cppcms::templates::generator::context ctx;
		if(argc >= 4 && std::string(argv[3]) == "-s") {
			ctx.skin = std::string(argv[4]);
		}
		p.write(ctx, std::cout);
	}
	return 0;
}
