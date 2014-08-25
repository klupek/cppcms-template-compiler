#include "parser.h"
namespace cppcms { namespace templates {
	static bool is_latin_letter(char c) {
		return ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' );
	}

	static bool is_digit(char c) {
		return ( c >= '0' && c <= '9' );
	}
	/*	
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
*/
	
	static std::string compress_html(const std::string& input) {
		std::string result(input.length()*2, 0);
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
			if(std::isprint(c)) {
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

	parser::parser(const std::string& input)
		: input_(input)
		, index_(0)
       		, failed_(0) {}
	
	parser& parser::try_token(const std::string& token) {
#ifdef PARSER_DEBUG
		std::cout << ">>>(" << failed_ << ") find token '" << token << "' at '" << input_.substr(index_, 20) << "'\n";
#endif
		if(!failed_ && input_.length()-index_ >= token.length() && input_.compare(index_, token.length(), token) == 0) {
			stack_.emplace_back(state_t { index_, token });
#ifdef PARSER_DEBUG
			std::cout << ">>> token " << stack_.back().token << std::endl;
#endif
			index_ += token.length();
		} else {
			failed_ ++;
		}
		return *this;
	}

	/* NAME is a sequence of Latin letters, digits and underscore starting with a letter. They represent identifiers and can be defined by regular expression such as: [a-zA-Z][a-zA-Z0-9_]* */
	parser& parser::try_name() {
#ifdef PARSER_DEBUG
		std::cout << ">>>(" << failed_ << ") find name at '" << input_.substr(index_, 20) << "'\n";
#endif
		if(!failed_ && index_ < input_.length()) {
			size_t start = index_;
			if(is_latin_letter(input_[index_]) || input_[index_] == '_') {
				index_++;
				for(;index_ < input_.length() && ( is_latin_letter(input_[index_]) || is_digit(input_[index_]) || input_[index_] == '_'); ++index_);
				stack_.emplace_back(state_t { start, std::string(&input_[start], &input_[index_]) });
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
		std::cout << ">>>(" << failed_ << ") find string at '" << input_.substr(index_, 20) << "'\n";
#endif
		if(!failed_ && index_ < input_.length()) {
			size_t start = index_;
			if(input_[index_] == '"') {
				bool escaped = false;
				++index_;
				for(; index_ < input_.length() && (input_[index_] != '"' || escaped); ++index_) {
					if(escaped)
						escaped = false;
					else if(input_[index_] == '\\') 
						escaped = true;
				}
				if(index_ == input_.length()) { // '"' was not found
					index_ = start;
					raise("expected \", found EOF instead");
				} else {
					stack_.emplace_back(state_t { start, std::string(&input_[start], &input_[index_+1]) }); // FIXME: decode?
#ifdef PARSER_DEBUG
					std::cout << ">>> string " << stack_.back().token << std::endl;
#endif
					index_++; // move to next character, past '"'
				}
			} else {
#ifdef PARSER_DEBUG
				std::cout << ">>> string failed at " << index_ << std::endl;
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
		std::cout << ">>>(" << failed_ << ") find number at '" << input_.substr(index_, 20) << "'\n";
#endif
		if(!failed_ && index_ < input_.length()) {
			size_t start = index_;
			if(input_[index_] == '-' || input_[index_] == '+')
				index_++;
			if(input_[index_] >= '0' && input_[index_] <= '9') {
				bool dot = false;
				for(; (input_[index_] >= '0' && input_[index_] <= '9') || (!dot && input_[index_] == '.'); ++index_) {
					if(input_[index_] == '.')
						dot = true;
				}
				stack_.emplace_back(state_t{ start, std::string(&input_[start], &input_[index_]) });				
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
		std::cout << ">>>(" << failed_ << ") find var at '" << input_.substr(index_, 20) << "'\n";
#endif
		if(!failed_ && index_ < input_.length()) {
			auto stack_copy = stack_;
			size_t start = index_, end = index_;

			// parse *name((\.|->)name)
			if(input_[index_] == '*')
				index_++;
			while(try_name()) {
				const size_t right = input_.length() - index_;
				end = index_;
				if(right >= 1 && input_[index_] == '.') {
					index_ ++;
				} else if(right >= 2 && input_[index_] == '-' && input_[index_+1] == '>') {
					index_ += 2;
				} else {
					break;
				}
			}
			back(1); // remove 
			
			std::swap(stack_, stack_copy);
			
			if(end != start) {
				const size_t right = input_.length() - end;
				
				// parse () at the end
				if(right >= 2 && input_[end] == '(' && input_[end+1] == ')') 
					end += 2;
				stack_.emplace_back(state_t { start, std::string(&input_[start], &input_[end]) });
#ifdef PARSER_DEBUG
				std::cout << ">>> var " << stack_.back().token << std::endl;
#endif
				index_ = end;
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
		std::cout << ">>>(" << failed_ << ") find cvar at '" << input_.substr(index_, 20) << "'\n";
#endif
		if(!failed_ && index_ < input_.length()) {
			push();
			size_t start = index_, end = index_;
			if(try_variable()) {
				const std::string var = get(-1);
				end = index_;
				while(skipws(false).try_token("|").skipws(false).try_filter()) { 
					end = index_;
					details_.emplace(detail_t { "complex_variable", get(-1) });
				}
				
				back(4);
				pop();
				
				stack_.emplace_back(state_t {start, std::string(&input_[start], &input_[end]) });				
				details_.emplace(detail_t { "complex_variable_name", var });
#ifdef PARSER_DEBUG
				std::cout << ">>> complex " << stack_.back().token << std::endl;
#endif
				index_  = end;
			} 
		} else {
			failed_++;
		}
		return *this;
	}	

	// IDENTIFIER is a sequence of NAME separated by the symbol ::. No blanks are allowed. For example: data::page
	parser& parser::try_identifier() {
#ifdef PARSER_DEBUG
		std::cout << ">>>(" << failed_ << ") find id at '" << input_.substr(index_, 20) << "'\n";
#endif
		if(!failed_ && index_ < input_.length()) {
			auto stack_copy = stack_;
			push();
			size_t start = index_, end = index_;

			while(try_name()) {
				const size_t right = input_.length() - index_;
				end = index_;
				if(right >= 2 && input_[index_] == ':' && input_[index_+1] == ':') {
					index_ += 2;
				} else {
					reset();
					break;
				}
			}
			std::swap(stack_, stack_copy);
			if(end != start) {
				stack_.emplace_back(state_t { start, std::string(&input_[start], &input_[end]) });
#ifdef PARSER_DEBUG
				std::cout << ">>> id " << stack_.back().token << std::endl;
#endif
				index_ = end;
			} else {
				failed_++;
			}

			pop();
		} else {
			failed_++;
		}
		return *this;
	}
	parser& parser::try_argument_list() {
		if(!failed_ && index_ < input_.length()) {			
			size_t start = index_, end = index_;
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
			end = index_;
			std::swap(stack_, stack_copy);
			stack_.emplace_back(state_t { start, std::string(&input_[start], &input_[end]) });
		} else {
			failed_++;
		}
		return *this;

	}
	// [ 'ext' ] NAME [ '(' ( VARIABLE | STRING | NUMBER ) [ ',' ( VARIABLE | STRING | NUMBER ) ] ... ]
	parser& parser::try_filter() {
#ifdef PARSER_DEBUG
		std::cout << ">>>(" << failed_ << ") find filter at '" << input_.substr(index_, 20) << "'\n";
#endif
		if(!failed_ && index_ < input_.length()) {
			size_t start = index_;
			if(try_token_ws("ext")) {
			} else {
				back(2);
			}
			if(try_name()) {
				try_argument_list();
				stack_.pop_back();
				stack_.emplace_back(state_t { start, std::string(&input_[start],&input_[index_]) });
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
		if(!failed_ && index_ < input_.length()) {
			size_t r = input_.find(token, index_);
			if(r == std::string::npos) {
				failed_ +=2;
			} else {
				stack_.emplace_back(state_t { index_, input_.substr(index_, r - index_) } );
				stack_.emplace_back(state_t { r, token });
				index_ = r + token.length();
			}
		} else {
			failed_+=2;
		}
		return *this;
	}

	parser& parser::skipws(bool require) {
		if(!failed_ && index_ < input_.length()) {
			const size_t start = index_;
			for(;index_ < input_.length() && std::isspace(input_[index_]); ++index_);
			stack_.emplace_back( state_t { start, input_.substr(start, index_ - start) });
#ifdef PARSER_DEBUG
			std::cout << ">>> skipws from " << start << " to " << index_ << std::endl;
#endif
			if(require && index_ == start)
				failed_++;
		} else {
			failed_ ++;
		}
		return *this;
	}

	parser& parser::try_comma() {
		if(!failed_) {
			size_t start = index_;
			skipws(false);
			try_token(",");
			skipws(false);
			if(failed_) {
				back(3);
				failed_++;
			} else {
				size_t end = index_;
				back(3);
				stack_.emplace_back(state_t { start, std::string(&input_[start], &input_[end]) });
				index_ = end;
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
			if(index_ < input_.length() && input_[index_] == '\n')
				index_++;
		}
		return *this;
	}

	parser& parser::skip_to_end() {
		if(!failed_) {
			stack_.emplace_back( state_t { index_, input_.substr(index_) });
			index_ = input_.length();
		} else {
			failed_++;
		}
		return *this;
	}

	parser& parser::try_parenthesis_expression() {
#ifdef PARSER_DEBUG
		std::cout << ">>>(" << failed_ << ") find parenthesis expression at '" << input_.substr(index_, 20) << "'\n";
#endif
		if(!failed_ && index_ < input_.length() && input_[index_] == '(') {
			size_t start = index_;
			int bracket_count = 1;
			bool escaped = false, string = false, string2 = false;
			++index_;
			for(; index_ < input_.length() && bracket_count > 0; ++index_) {
				const char c = input_[index_];
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
				stack_.emplace_back(state_t { start, std::string(&input_[start], &input_[index_]) });
			} else {
				index_ = start;
				failed_++;
			}
		} else {
			failed_ ++;
		} 
		return *this;
	}


	void parser::raise(const std::string& msg) {
		const int context = 70;
		const std::string left = ( index_ >= context ? input_.substr(index_ - context, context) : input_.substr(0, index_) );
		const std::string right = ( index_ + context < input_.size() ? input_.substr(index_, context) : input_.substr(index_) );
		throw std::runtime_error("Parse error at position " + boost::lexical_cast<std::string>(index_) + " near '\e[1;32m" + left + "\e[1;31m" + right + "\e[0m': " + msg); 
	}

	bool parser::failed() const {
		return failed_ != 0;
	}	

	parser::operator bool() const {
		return !failed(); 
	}

	bool parser::finished() const {
		return ( stack_.empty() || stack_.back().index == index_ ) && index_ == input_.length(); 
	}

	parser::details_t& parser::details() { return details_; }
	
	parser& parser::back(size_t n) {
		if(n > failed_ + stack_.size())
			throw std::logic_error("Attempt to clear more tokens then stack_.size()+failed_");

		if(n >= failed_) {
			n -= failed_;
			failed_ = 0;
		
			while(n-- > 0) {
				index_ = stack_.back().index;
				stack_.pop_back();
			}
		} else {
			failed_ -= n;
		}
		return *this;
	}


	void parser::push() {
		state_stack_.push({index_,failed_});
	}

	void parser::pop() {
		if(state_stack_.empty())
			throw std::logic_error("Attempt to pop empty state stack");
		state_stack_.pop();
	}

	parser& parser::reset() {
		if(state_stack_.empty())
			throw std::logic_error("Attempt to reset with empty state stack");
		index_ = state_stack_.top().first;
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

	void template_parser::parse() {
		bool last_item_was_code = true;
		auto is_spaces = [](const std::string& input) {
			return std::all_of(input.begin(), input.end(), [](const char c) {
				return c == ' ' || c == '\t' || c == '\n' || c == '\r';
			});
		};
		while(!p.finished() && !p.failed()) {
			p.push();
			if(p.reset().skip_to("<%")) { // [ <html><blah>..., <% ] = 2
#ifdef PARSER_DEBUG
				std::cout << ">>> main -> <%\n";
#endif
				if(!last_item_was_code || !is_spaces(p.get(-2))) {
					size_t pos = p.get(-2).find("%>");
					if(pos != std::string::npos) {
						p.back(2).skip_to("%>").back(1).raise("unexpected %>");
					}
					add_html(p.get(-2));
				}

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
				last_item_was_code  = true;
			} else if(p.reset().skip_to("%>")) {
				p.reset().raise("found unexpected %>");
			} else if(p.reset().skip_to_end()) { // [ <blah><blah>EOF ]
#ifdef PARSER_DEBUG
				std::cout << ">>> main -> skip to end\n";
#endif
				add_html(p.get(-1));
				last_item_was_code = false;
			} else {
				p.reset().raise("expected <%=, <% or EOF");
			}
			p.pop();
		}
	}

	bool template_parser::try_flow_expression() {
		p.push();
		// ( 'if' | 'elif' ) [ 'not' ] [ 'empty' ] ( VARIABLE | 'rtl' )  
		if(p.try_token_ws("if") || p.back(2).try_token_ws("elif")) {
			const std::string verb = p.get(-2);
			bool negate = false;
			if(p.try_token_ws("not")) {
				negate = true;
			} else {
				p.back(2);
			}
			if(p.try_token_ws("empty").try_variable_ws()) { // [ empty, \s+, VARIABLE, \s+ ]				
				const std::string var = p.get(-2);
				std::cout << "flow: " << verb << " " << (negate ? "not " : "") << "empty: " << var << std::endl;
			} else if(p.back(4).try_variable_ws()) { // [ VAR, \s+ ]
				const std::string var = p.get(-2);
				std::cout << "flow: " << verb << " " << (negate ? "not " : "") << ":" << var << std::endl;
			} else if(p.back(2).try_token("(") && p.back(1).try_parenthesis_expression().skipws(false)) { // [ (, \s*, expr, \s* ]
				const std::string expr = p.get(-2);
				std::cout << "flow: " << verb << " c++ expr: " << expr << std::endl;
			} else {
				p.raise("expected [not] [empty] ([variable]|rtl) or ( c++ expr )");
			}
			if(!p.skipws(false).try_token("%>")) {
				p.raise("expected %>");
			}
		} else if(p.reset().try_token_ws("else").try_token_ws("%>")) {
			std::cout << "flow: else\n";
		// 'foreach' NAME ['as' IDENTIFIER ] [ 'rowid' IDENTIFIER [ 'from' NUMBER ] ] [ 'reverse' ] 'in' VARIABLE
		} else if(p.reset().try_token_ws("foreach").try_name_ws()) {
			const std::string item_name = p.get(-2);
			std::string as("(default)"), rowid("(none)"), variable;
			bool reverse = false;
			int from = 0;

			if(p.try_token_ws("as").try_identifier_ws()) {
				as = p.get(-2);
			} else {
				p.back(4);
			}
			if(p.try_token_ws("rowid").try_name_ws()) { // docs say IDENTIFIER, but local variable should be NAME
				rowid = p.get(-2);
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
				variable = p.get(-3);
			} else {
				p.raise("expected in VARIABLE %>");
			}

			std::cout << "flow: foreach (" << item_name << " in " << variable << "; rowid " << rowid << ", reverse " << reverse << ", as " << as << ", from " << from << "\n";
		} else if(p.reset().try_token_ws("item").try_token("%>")) {
			std::cout << "flow: item\n";
		} else if(p.reset().try_token_ws("empty").try_token("%>")) {
			std::cout << "flow: empty\n";
		} else if(p.reset().try_token_ws("separator").try_token("%>")) {
			std::cout << "flow: separator\n";
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
			if(what.empty() || current_->sysname() == what) { // FIXME: what.empty() for debugging only
				if(!what.empty() && current_->parent()) {
					current_ = current_->parent();
				} else {
					// as below
					// p.back(3).raise("unexpected end");
				}
			} else {
				// FIXME
				// disabled until every block is supported
//				p.back(3).raise("expected 'end " + current_->sysname() + "'");
			}
#ifdef PARSER_TRACE
			std::cout << "flow: end " << name << "\n";
#endif

		// 'cache' ( VARIABLE | STRING ) [ 'for' NUMBER ] ['on' 'miss' VARIABLE() ] [ 'no' 'triggers' ] [ 'no' 'recording' ]
		} else if(p.reset().try_token_ws("cache")) {
			std::string name, miss;
			int _for = -1;
			bool no_triggers = false, no_recording = false;
			if(p.try_variable_ws() || p.back(2).try_string_ws()) {
				name = p.get(-2);
			} else {
				p.raise("expected VARIABLE or STRING");
			}

			if(p.try_token_ws("for").try_number_ws()) {
				_for = p.get<int>(-2);
			} else {
				p.back(4);
			}

			if(p.try_token_ws("on").try_token_ws("miss").try_variable()) { // TODO: () is required at the end of expression, but try_variable already consumes it
				miss = p.get(-1);
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

			std::cout << "flow: cache " << name << ", miss = " << miss << ", for " << _for << ", no_triggers = " << no_triggers << ", no_recording = " << no_recording << "\n";
		} else if(p.reset().try_token_ws("trigger")) {
			if(!p.try_variable() && !p.back(1).try_string()) {
				p.raise("expected STRING or VARIABLE");
			} else {
				const std::string name = p.get(-1);
				std::cout << "flow: trigger " << name << std::endl;
			}
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
			current_->as<ast::root_t>().add_skin(skin_name);
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
			if(p.try_token_nl("%>")) {
				current_ = current_->as<ast::root_t>().add_view(view_name, data_name, parent_name);
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
			current_ = current_->as<ast::view_t>().add_template(function_name, arguments);
#ifdef PARSER_TRACE
			std::cout << "global: template " << function_name << "\n";
#endif
		} else if(p.reset().try_token_ws("c++").skip_to("%>")) { // [ c++, \s+, cppcode, %> ] = 4
			add_cpp(p.get(-2));
		} else if(p.reset().try_token_ws("html").try_token("%>") || p.back(3).try_token_ws("xhtml").try_token("%>")) { // [ html|xhtml, \s+, %> ]
			const std::string mode = p.get(-3);
			std::cout << "global: mode " << mode << std::endl;
		} else {
			p.reset();
			return false;
		}
		p.pop();
		return true;
	}
		
	ast::using_options_t template_parser::parse_using_options(std::vector<std::string>& variables) {
		if(p.try_token_ws("using")) {
			while(p.skipws(false).try_complex_variable()) { // [ \s*, variable ]					
				variables.emplace_back(p.get(-1));
				std::cout << "\tvariable " << p.get(-1) << std::endl;
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
			while(!p.details().empty()) {
				const auto& op = p.details().top();
				if(op.what == "complex_variable_name") {
					options.emplace_back(ast::using_option_t { op.item, std::vector<std::string>(filters.rbegin(), filters.rend()) });
					filters.clear();
				} else { // what == complex_variable
					filters.emplace_back(op.item);
				}
				p.details().pop();
			}
			return ast::using_options_t(options.rbegin(), options.rend());
		} else {
			p.back(2);
			return ast::using_options_t();
		}
	}

	bool template_parser::try_render_expression() {
		p.push();
		if(p.try_token_ws("gt").try_string_ws() || p.back(4).try_token_ws("pgt").try_string_ws()) { // [ gt|pgt, \s+, string, \s+ ]
			const std::string fmt = p.get(-2);
			std::vector<std::string> variables;
			auto options = parse_using_options(variables);
			if(!p.skipws(false).try_token("%>")) {
				p.raise("expected %> after gt expression");
			}

			current_ = current_->as<ast::has_children>().add<ast::fmt_function_t>("gt", fmt, options);
#ifdef PARSER_TRACE
			std::cout << "render: gt " << fmt << "\n";
#endif
		} else if(p.reset().try_token_ws("ngt").try_string().try_comma().try_string().try_comma().try_variable_ws()) { // [ ngt, \s+, STRING, ',', STRING, ',', VARIABLE, \s+ ]
			const std::string singular = p.get(-6);
			const std::string plural = p.get(-4);
			const std::string variable = p.get(-2);
			std::vector<std::string> variables;
			auto options = parse_using_options(variables);
			if(!p.skipws(false).try_token("%>")) {
				p.raise("expected %> after gt expression");
			}

			current_ = current_->as<ast::has_children>().add<ast::ngt_t>(singular, plural, variable, options);
#ifdef PARSER_TRACE
			std::cout << "render: ngt " << singular << "/" << plural << "/" << variable << std::endl;
#endif
		} else if(p.reset().try_token_ws("url").try_string_ws()) { // [ url, \s+, STRING, \s+ ]
			const std::string url = p.get(-2);
			std::vector<std::string> variables;
			auto options = parse_using_options(variables);
			if(!p.skipws(false).try_token("%>")) {
				p.raise("expected %> after gt expression");
			}

			current_ = current_->as<ast::has_children>().add<ast::fmt_function_t>("url", url, options);			
#ifdef PARSER_TRACE
			std::cout << "render: url " << url << std::endl;
#endif
		} else if(p.reset().try_token_ws("include").try_identifier()) { // [ include, \s+, identifier ]
			const std::string id = p.get(-1);
			std::string from, _using, with;
			p.skipws(false);
			p.try_argument_list(); // [ argument_list ], cant fail
			std::vector<std::string> arguments;
			while(!p.details().empty()) {
				arguments.insert(arguments.begin(), p.details().top().item);
				p.details().pop();
			}

			const std::string alist = p.get(-1);
			if(p.skipws(true).try_token_ws("from").try_identifier_ws()) { // [ \s+, from, \s+, ID, \s+ ]	
				from = p.get(-2);
			} else if(p.back(4).try_token_ws("using").try_identifier_ws()) { // [ \s+, using, \s+, ID, \s+ ]
				_using = p.get(-2);
				if(p.try_token_ws("with").try_variable_ws()) { // [ with, \s+, VAR, \s+ ]
					with = p.get(-2);
				} else {
					p.back(4);
				} 
			} else {
				p.back(5);
			}
			if(!p.skipws(false).try_token("%>")) {
				p.raise("expected %> after gt expression");
			}
			current_ = current_->as<ast::has_children>().add<ast::include_t>(id, from, _using, with, arguments);
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
			std::string id, with, as;
			id = p.get(-2);
			if(p.try_token_ws("with").try_variable_ws()) {
				with = p.get(-2);
			} else {
				p.back(4);
			}
			if(p.try_token_ws("as").try_identifier_ws()) {
				as = p.get(-2);
			} else {
				p.back(4).raise("expected 'as' IDENTIFIER");
			}
			if(!p.skipws(false).try_token("%>")) {
				p.raise("expected %> after gt expression");
			}

			// save to tree
			current_ = current_->as<ast::has_children>().add<ast::using_t>(id, with, as);
#ifdef PARSER_TRACE
			std::cout << "render: using " << id << std::endl;
			std::cout << "\twith " << (with.empty() ? "(current)" : with) << std::endl;
			std::cout << "\tas " << as << std::endl;
#endif
		} else if(p.reset().try_token_ws("form").try_name_ws().try_variable_ws().try_token("%>")) { // [ form, \s+, NAME, \s+, VAR, \s+, %> ]
			const std::string name = p.get(-5), var = p.get(-3);
			current_ = current_->as<ast::has_children>().add<ast::form_t>(name, var);
#ifdef PARSER_TRACE
			std::cout << "render: form, name = " << name << ", var = " << var << "\n";
#endif
		} else if(p.reset().try_token_ws("csrf")) {
			std::string type;
			if(p.try_name_ws().try_token("%>")) { // [ csrf, \s+, NAME, \s+, %> ]
				type = p.get(-3);
			} else if(!p.back(3).try_token("%>")) {
				p.raise("expected csrf style(type) or %>");
			}
			// save to tree
			current_ = current_->as<ast::has_children>().add<ast::csrf_t>(type);
#ifdef PARSER_TRACE
			std::cout << "render: csrf " << ( type.empty() ? "(default)" : type ) << "\n";
#endif
		// 'render' [ ( VARIABLE | STRING ) , ] ( VARIABLE | STRING ) [ 'with' VARIABLE ] 
		} else if(p.reset().try_token_ws("render")) {
			if(p.try_variable() || p.back(1).try_string()) {
				std::string view = p.get(-1), skin, with;
				if(p.try_comma().try_variable_ws() || p.back(2).try_string_ws()) {
					skin = view;
					view = p.get(-2);
				} else {
					p.back(3).skipws(false);
				}
				if(p.try_token_ws("with").try_variable_ws()) {
					with = p.get(-2);
				} else {
					p.back(4);
				}
				if(!p.try_token("%>")) {
					p.raise("expected %>");
				}
				// save to tree
				current_ = current_->as<ast::has_children>().add<ast::render_t>(skin, view, with);
#ifdef PARSER_TRACE
				std::cout << "render: render\n\tskin = " << (arg2.empty() ? "(default)" : arg1 ) << "\n\tview = " << (arg2.empty() ? arg1 : arg2) << std::endl;
				if(!with.empty())
					std::cout << "\twith " << with << std::endl;
#endif
			} else {
				p.raise("expected STRING or VARIABLE");
			}
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
			const std::string expr = p.details().top().item;
			std::vector<std::string> filters;
			p.details().pop();
			while(!p.details().empty() && p.details().top().what == "complex_variable") {
				filters.emplace_back(p.details().top().item);
				p.details().pop();
			}
			current_ = current_->as<ast::has_children>().add<ast::variable_t>(expr, filters);
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
		std::cout << "html: " << compress_html(html) << std::endl;
	}

	void template_parser::add_cpp(const std::string& cpp) {
		if(current_->is_a<ast::root_t>())
			current_ = current_->as<ast::root_t>().add_cpp(cpp);
		else
			current_ = current_->as<ast::has_children>().add<ast::cppcode_t>(cpp);
#ifdef PARSER_TRACE
		std::cout << "cpp: " << cpp << std::endl;
#endif
	}

	namespace ast {
		base_t::base_t(const std::string& sysname, bool block, base_ptr parent)
			: sysname_(sysname)
	       		, parent_(parent) 
			, block_(block) {}
		
		base_ptr base_t::parent() { return parent_; }
		
		const std::string& base_t::sysname() const {
			return sysname_;
		}

		bool base_t::block() const { return block_; }

		root_t::root_t() 
			: base_t("root", true, nullptr)		
 			, current_skin(skins.end()) {}

		void root_t::add_skin(const std::string& name) {
			current_skin = skins.emplace( name, view_set_t() ).first;
		}

		base_ptr root_t::add_cpp(const std::string& code) {
			codes.emplace_back(code);
			return shared_from_this();
		}
			
		base_ptr root_t::add_view(const std::string& name, const std::string& data, const std::string& parent) {
			if(current_skin == skins.end())
				throw std::runtime_error("view must be inside skin");
			
			return current_skin->second.emplace(
				name, std::make_shared<view_t>(name, data, parent, shared_from_this())
			).first->second;
		}

		void root_t::write(std::ostream& o) {
			for(const skins_t::value_type& skin : skins) {
				for(const view_set_t::value_type& view : skin.second) {
					view.second->write(o);
				}
			}	       
		}
			
		void root_t::dump(std::ostream& o, int tabs) {
			std::string p(tabs, '\t');
			o << p << "root with " << codes.size() << " codes [\n";
			for(const skins_t::value_type& skin : skins) {
				o << p << "\tskin " << skin.first << " with " << skin.second.size() << " views [\n";
				for(const view_set_t::value_type& view : skin.second) {
					view.second->dump(o, tabs+2);
				}
				o << p << "\t]\n";
			}
			o << p << "]; codes = [\n";
			for(const std::string& code : codes)
				o << p << "\t" << code << std::endl;
			o << p << "];\n";
		}

		void view_t::write(std::ostream& /* o */) {
		}

		void template_t::write(std::ostream& /* o */) {
		}

		base_ptr view_t::add_template(const std::string& name, const std::string& arguments) {
			return templates.emplace(
				name, std::make_shared<template_t>(name, arguments, shared_from_this())
			).first->second;
		}
		view_t::view_t(const std::string& name, const std::string& data, const std::string& master, base_ptr parent)
			: base_t("view", true, parent)
			, name_(name)
			, data_(data)
			, master_(master) {}
		
		void view_t::dump(std::ostream& o, int tabs) {
			const std::string p(tabs, '\t');
			o << p << "view " << name_ << " uses " << data_ << " extends " << ( master_.empty() ? "(default)" : master_ ) 
				<< " with " << templates.size() << " templates {\n";
			for(const templates_t::value_type& templ : templates) {
				templ.second->dump(o, tabs+1);
			}
			o << p << "}\n";
		}
			
		template_t::template_t(const std::string& name, const std::string& arguments, base_ptr parent) 
			: has_children("template", true, parent)
	       		, name_(name) 
			, arguments_(arguments) {}

		void template_t::dump(std::ostream& o, int tabs) {
			const std::string p(tabs, '\t');
			o << p << "template " << name_ << " with arguments " << arguments_ << " and " << children.size() << " children [\n";
			for(const base_ptr& child : children)
				child->dump(o, tabs+1);
			o << p << "]\n";
		}
		
		cppcode_t::cppcode_t(const std::string& code, base_ptr parent)
			: base_t("c++", false, parent)
			, code_(code) {}

		void cppcode_t::dump(std::ostream& o, int tabs) {
			const std::string p(tabs, '\t');
			o << p << "c++: " << code_ << std::endl;
		}

		void cppcode_t::write(std::ostream& /* o */) {
		}
		
		variable_t::variable_t(const std::string& name, const std::vector<std::string>& filters, base_ptr parent)
			: base_t("variable", false, parent)
			, name_(name)
			, filters_(filters) {}

		void variable_t::dump(std::ostream& o, int tabs) {
			const std::string p(tabs, '\t');
			o << p << "variable: " << name_;
			if(filters_.empty())
				o << " without filters\n";
			else {
				o << " with filters: "; 
				for(const std::string& filter : filters_) 
					o << " | " << filter;
				o << std::endl;
			}
		}

		void variable_t::write(std::ostream& /* o */) {
		}
			
		fmt_function_t::fmt_function_t(	const std::string& name, 
						const std::string& fmt, 
						const using_options_t& uos, 
						base_ptr parent) 
			: base_t(name, false, parent)
			, name_(name)
			, fmt_(fmt)
			, using_options_(uos) {}
		
		void fmt_function_t::dump(std::ostream& o, int tabs) {
			const std::string p(tabs, '\t');
			o << p << "fmt function " << name_ << ": " << fmt_ << std::endl;
			if(using_options_.empty()) {
				o << p << "\twithout using\n";
			} else {
				o << p << "\twith using options:\n"; 
				for(const using_option_t& uo : using_options_) {
					o << p << "\t\t" << uo.variable;
					if(!uo.filters.empty()) {
						o << " with filters ";
						for(const std::string& filter : uo.filters)  {
							o << " | " << filter;
						}
					}
					o << std::endl;
				}
			}
		}

		void fmt_function_t::write(std::ostream& /* o */) {
		}
		
		ngt_t::ngt_t( 	const std::string& singular, 
				const std::string& plural,
				const std::string& variable,
				const using_options_t& uos, 
				base_ptr parent)
			: base_t("ngt", false, parent)
			, singular_(singular)
			, plural_(plural)
			, variable_(variable) 
			, using_options_(uos) {}
		
		void ngt_t::dump(std::ostream& o, int tabs) {
			const std::string p(tabs, '\t');
			o << p << "fmt function ngt: " << singular_ << "/" << plural_ << " with variable " << variable_ <<  std::endl;
			if(using_options_.empty()) {
				o << p << "\twithout using\n";
			} else {
				o << p << "\twith using options:\n"; 
				for(const using_option_t& uo : using_options_) {
					o << p << "\t\t" << uo.variable;
					if(!uo.filters.empty()) {
						o << " with filters ";
						for(const std::string& filter : uo.filters)  {
							o << " | " << filter;
						}
					}
					o << std::endl;
				}
			}
		}

		void ngt_t::write(std::ostream& /* o */) {
		}
			
		include_t::include_t(	const std::string& name, const std::string& from, 
					const std::string& _using, const std::string& with, 
					const std::vector<std::string>& arguments, base_ptr parent) 
			: base_t("include", false, parent) 
			, name_(name)
			, from_(from)
			, using_(_using) 
			, with_(with)
			, arguments_(arguments) {}
		
		void include_t::dump(std::ostream& o, int tabs) {
			const std::string p(tabs, '\t');
			o << p << "include " << name_;

			if(from_.empty()) {
				if(!using_.empty()) {
					o << " using " << using_;
					if(!with_.empty())
						o << " with " << with_;
					else
						o << " with (this content)";
				} else {
					o << " from (self)";
				}
			} else {
				o << " from " << from_;
			}
			o << std::endl;
			if(arguments_.empty()) {
				o << p << "\twithout arguments\n";
			} else {
				o << p << "\twith arguments (";
				for(const std::string& arg : arguments_)
					o << arg << ", ";
				o << ")\n";
			}
		}

		void include_t::write(std::ostream& /* o */) {
		}

		form_t::form_t(const std::string& style, const std::string& name, base_ptr parent)
			: base_t("form", false, parent)
			, style_(style)
			, name_(name) {}
		
		void form_t::dump(std::ostream& o, int tabs) {
			const std::string p(tabs, '\t');
			o << p << "form style = " << style_ << " using variable " << name_ << std::endl;
		}
		
		void form_t::write(std::ostream& /* o */) {
		}
		
		csrf_t::csrf_t(const std::string& style, base_ptr parent)
			: base_t("csrf", false, parent)
			, style_(style) {}
		
		void csrf_t::dump(std::ostream& o, int tabs) {
			const std::string p(tabs, '\t');
			o << p << "csrf style = " << (style_.empty() ? "(default)" : style_ ) << std::endl;
		}
		
		void csrf_t::write(std::ostream& /* o */) {
		}
		
		render_t::render_t(const std::string& skin, const std::string& view, const std::string& with, base_ptr parent)
			: base_t("render", false, parent)
			, skin_(skin)
			, view_(view)
			, with_(with) {}
		
		void render_t::dump(std::ostream& o, int tabs) {
			const std::string p(tabs, '\t');
			o << p << "render skin = " << (skin_.empty() ? "(current)" : skin_ )
				<< ", view = " << view_ << " with " << ( with_.empty() ? "(current)" : with_ ) << " content\n";
		}
		
		void render_t::write(std::ostream& /* o */) {
		}
			
		using_t::using_t(const std::string& id, const std::string& with, const std::string& as, base_ptr parent)
			: has_children("using", true, parent)
			, id_(id)
			, with_(with)
			, as_(as) {}
		
		void using_t::dump(std::ostream& o, int tabs) {
			const std::string p(tabs, '\t');
			o << p << "using view type " << id_ << " as " << as_ << " with " << (with_.empty() ? "(current)" : with_) << " content [\n";
			for(const base_ptr& child : children)
				child->dump(o, tabs+1);
			o << p << "]\n";
		}
		
		void using_t::write(std::ostream& /* o */) {
		}
			

	}
}}

#include <iostream>
#include <streambuf>
#include <fstream>

int main(int /*argc*/, char **argv) {
	std::ifstream ifs(argv[1]);
	const std::string input(std::istreambuf_iterator<char>{ifs}, std::istreambuf_iterator<char>{});
	cppcms::templates::template_parser p(input);
	p.parse();
	p.tree()->dump(std::cout);
	return 0;
}
