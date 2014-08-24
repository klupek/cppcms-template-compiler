#include "parser.h"
namespace cppcms { namespace templates {
	static bool is_latin_letter(char c) {
		return ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' );
	}

	static bool is_digit(char c) {
		return ( c >= '0' && c <= '9' );
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
					stack_.emplace_back(state_t { start, decode_escaped_string(std::string(&input_[start+1], &input_[index_])) });
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
				end = index_;
				while(skipws(false).try_token("|").skipws(false).try_filter()) { 
					end = index_;
				}
				
				back(4);
				pop();
				
				stack_.emplace_back(state_t {start, std::string(&input_[start], &input_[end]) });				
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
						} else if(back(1).try_string()) {
						} else if(back(1).try_number()) {
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
		, tree_(std::make_shared<ast::root_t>()) {}
		

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
				if(!last_item_was_code || !is_spaces(p.get(-2))) {
					size_t pos = p.get(-2).find("%>");
					if(pos != std::string::npos) {
						p.back(2).skip_to("%>").back(1).raise("unexpected %>");
					}
					add_html(p.get(-2));
				}

				p.push();
				if(p.try_token_ws("=")) { // [ <html><blah>..., <%, = ] = 3
					if(!try_variable_expression()) {
						p.raise("expected variable expression");
					}
				} else if(p.reset().skipws(true)) { // [ <html><blah>..., <%, \s+ ] = 3
					if(!try_flow_expression() && !try_global_expression() && !try_render_expression()) {
						p.raise("expected c++, global, render or flow expression");
					}
				}
				p.pop();
				last_item_was_code  = true;
			} else if(p.reset().skip_to("%>")) {
				p.reset().raise("found unexpected %>");
			} else if(p.reset().skip_to_end()) { // [ <blah><blah>EOF ]
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

		if(p.try_token_ws("if")) {
			std::cout << "flow: if\n";
		} else if(p.reset().try_token_ws("elif")) {
			std::cout << "flow: elif\n";
		} else if(p.reset().try_token_ws("else")) {
			std::cout << "flow: else\n";
		} else if(p.reset().try_token_ws("foreach")) {
			std::cout << "flow: foreach\n";
		} else if(p.reset().try_token_ws("item")) {
			std::cout << "flow: item\n";
		} else if(p.reset().try_token_ws("empty")) {
			std::cout << "flow: empty\n";
		} else if(p.reset().try_token_ws("separator")) {
			std::cout << "flow: separator\n";
		} else if(p.reset().try_token_ws("end")) {
			if(p.try_name_ws()) {
				const std::string name = p.get(-2);
				std::cout << "flow: end " << name << "\n";
			} else {
				std::cout << "flow: end\n";
				p.back(2);
			}
			if(!p.try_token("%>")) {
				p.raise("expected %> after end [whatever]");
			}
		} else if(p.reset().try_token_ws("cache")) {
			std::cout << "flow: cache\n";
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
			if(p.try_token_nl("%>")) { // [ skin, \s+, %> ]
				skin_name = "__default__";
			} else if(p.reset().try_name_ws().try_token_nl("%>")) { // [ skin, \s+, skinname, \s+, %> ]
				skin_name = p.get(-3);
			} else {
				p.reset().raise("expected %> or skin name");
			}
			p.pop();
			std::cout << "global: skin " << skin_name << "\n";
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
				std::cout << "global: view " << view_name << "/" << data_name << "/" << parent_name << "\n";
			} else {
				p.reset().raise("expected %> after view definition");
			}
			p.pop();
		} else if(p.reset().try_token_ws("template").skip_to("%>")) { // [ template, \s+, code  %> ]
			const std::string function_name = p.get(-2);
			std::cout << "global: template " << function_name << "\n";
		} else if(p.reset().try_token_ws("c++").skip_to("%>")) { // [ c++, \s+, cppcode, %> ] = 4
			add_cpp(p.get(-2));
		} else {
			p.reset();
			return false;
		}
		p.pop();
		return true;
	}
		
	void template_parser::parse_using_options(std::vector<std::string>& variables) {
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
		} else {
			p.back(2);
		}
	}

	bool template_parser::try_render_expression() {
		p.push();
		if(p.try_token_ws("gt").try_string_ws() || p.back(4).try_token_ws("pgt").try_string_ws()) { // [ gt|pgt, \s+, string, \s+ ]
			const std::string fmt = p.get(-2);
			std::cout << "render: gt " << fmt << "\n";
			std::vector<std::string> variables;
			parse_using_options(variables);
			if(!p.skipws(false).try_token("%>")) {
				p.raise("expected %> after gt expression");
			}
		} else if(p.reset().try_token_ws("ngt").try_string().try_comma().try_string().try_comma().try_variable_ws()) { // [ ngt, \s+, STRING, ',', STRING, ',', VARIABLE, \s+ ]
			const std::string singular = p.get(-6);
			const std::string plural = p.get(-4);
			const std::string variable = p.get(-2);
			std::vector<std::string> variables;
			std::cout << "render: ngt " << singular << "/" << plural << "/" << variable << std::endl;
			parse_using_options(variables);
			if(!p.skipws(false).try_token("%>")) {
				p.raise("expected %> after gt expression");
			}
		} else if(p.reset().try_token_ws("url").try_string_ws()) { // [ url, \s+, STRING, \s+ ]
			const std::string url = p.get(-2);
			std::cout << "render: url " << url << std::endl;
			std::vector<std::string> variables;
			parse_using_options(variables);
			if(!p.skipws(false).try_token("%>")) {
				p.raise("expected %> after gt expression");
			}
		} else if(p.reset().try_token_ws("include").try_identifier()) { // [ include, \s+, identifier, \s+ ]
			const std::string id = p.get(-2);
			std::cout << "render: include " << id << std::endl;
			p.skipws(false);
			p.try_argument_list(); // [ argument_list ], cant fail
			const std::string alist = p.get(-1);
			std::cout << "\tparameters " << alist << std::endl;
			if(p.skipws(true).try_token_ws("from").try_identifier_ws()) { // [ \s+, from, \s+, ID, \s+ ]	
				const std::string from = p.get(-2);
				std::cout << "\tfrom " << from << std::endl;
			} else if(p.back(4).try_token_ws("using").try_identifier_ws()) { // [ \s+, using, \s+, ID, \s+ ]
				const std::string _using = p.get(-2);
				std::cout << "\tusing " << _using << std::endl;
				if(p.try_token_ws("with").try_variable_ws()) { // [ with, \s+, VAR, \s+ ]
					const std::string variable = p.get(-2);
					std::cout << "\twith " << variable << std::endl;
				} else {
					p.back(4);
					std::cout << "\twith (default)"  << std::endl;
				} 
			} else {
				p.back(5);
			}
			if(!p.skipws(false).try_token("%>")) {
				p.raise("expected %> after gt expression");
			}
		} else if(p.reset().try_token_ws("using").try_identifier_ws()) { // 'using' IDENTIFIER  [ 'with' VARIABLE ] as IDENTIFIER  
			const std::string id = p.get(-2);
			std::cout << "render: using " << id << std::endl;
			if(p.try_token_ws("with").try_variable_ws()) {
				const std::string variable = p.get(-2);
				std::cout << "\twith " << variable << std::endl;
			} else {
				p.back(4);
				std::cout << "\twith (default)" << std::endl;
			}
			if(p.try_token_ws("as").try_identifier_ws()) {
				const std::string as = p.get(-2);
				std::cout << "\tas " << as << std::endl;
			} else {
				p.back(4).raise("expected 'as' IDENTIFIER");
			}
			if(!p.skipws(false).try_token("%>")) {
				p.raise("expected %> after gt expression");
			}
		} else if(p.reset().try_token_ws("form")) {
			std::cout << "render: form\n";
		} else if(p.reset().try_token_ws("csrf")) {
			std::cout << "render: csrf\n";
		} else {
			p.reset();
			return false;
		}
		p.pop();
		return true;
	}

	bool template_parser::try_variable_expression() {
		p.push();
		if(p.skip_to("%>")) { // [ variable xpr, %> ]
			const std::string expr = p.get<std::string>(-2); 
			std::cout << "variable: " << expr << std::endl;
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
		std::cout << "cpp: " << cpp << std::endl;
	}

	namespace ast {
		base_t::base_t(const std::string& sysname, base_ref parent)
			: sysname_(sysname)
	       		, parent_(parent) {}
		
		base_ref base_t::parent() { return parent_; }

		root_t::root_t() : base_t("root", *this) {}

		void root_t::write(std::ostream& o) {
			for(const skins_t::value_type& skin : skins) {
				for(const view_set_t::value_type& view : skin.second) {
					view.second->write(o);
				}
			}	       
		}

		void view_t::write(std::ostream& /* o */) {
		}

		void template_t::write(std::ostream& /* o */) {
		}

		view_t::view_t(const std::string& name, root_ref parent)
			: base_t("view", parent)
			, name(name) {}
			
		template_t::template_t(const std::string& name, view_ref parent) 
			: base_t("template", parent)
	       		, name_(name) {}

		void template_t::add(base_ptr what) {
			children.emplace_back(what);
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
	return 0;
}
