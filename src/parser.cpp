#include "parser.h"

#include <algorithm>
#include <boost/lexical_cast.hpp>

namespace cppcms { namespace templates {
	static bool is_latin_letter(char c) {
		return ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' );
	}

	static bool is_digit(char c) {
		return ( c >= '0' && c <= '9' );
	}
	
	static bool is_whitespace_string(const std::string& input) {
		return std::all_of(input.begin(), input.end(), [](char c) {
			return std::isspace(c);
		});
	}

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

	file_position_t parser::line() const { return source_.line(); }
	
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
	// Feature added: array subscript.	
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
				if(try_token("[").skipws(false)) {
					if(!try_string() && !back(1).try_number() && !back(1).try_variable()) {
						raise("expected STRING, VARIABLE or NUMBER as array subscript");
					}
					if(!skipws(false).try_token("]")) {
						raise("expected closing ']' after array subscript");
					}
				} else {
					back(2);
				}
				// scan for ([.]|->)(NAME) blocks 
				while(skipws(false).try_one_of_tokens({".","->"}).skipws(false).try_name()) {
					if(try_token("[").skipws(false)) {
						if(!try_string() && !back(1).try_number() && !back(1).try_variable()) {
							raise("expected STRING, VARIABLE or NUMBER as array subscript");
						}
						if(!skipws(false).try_token("]")) {
							raise("expected closing ']' after array subscript");
						}
					} else {
						back(2);
					}
					if(!try_token("()"))
					       back(1);
				}

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
				auto try_template_call_list = [this]() {
					if(try_token("<")) {
						std::string tmp;
						while(try_identifier().skipws(false).try_one_of_tokens({",", ">"}, tmp)) {
							if(tmp == ">") break;
						}
						if(tmp != ">") {
							raise("expected <identifier list>");
						}
					} else {
						back(1);
					}
					return *this;
				};
				try_template_call_list();
				while(try_token("::").try_name() && try_template_call_list());
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
		throw parse_error("Parse error at line " + line().filename + ":" + boost::lexical_cast<std::string>(line().line) + ", file offset " +
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
		throw parse_error("Error at file " + file.filename + ":" + boost::lexical_cast<std::string>(file.line) + " near '\e[1;32m" + left + "\e[1;31m" + right + "\e[0m': " + msg); 
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
		} catch(const parse_error&) {
			throw;
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
		} else if(p.reset().try_token_ws("template")) { // [ template, \s+, name, arguments, %> ]			
			std::string function_name, arguments;
			token_sink argsink(arguments);
			std::vector<expr::identifier> template_arguments;
			if(!p.try_name(function_name)) {
				p.raise("expected NAME(params...) %>");
			}
			if(p.try_token("<")) {
				std::string id, token;
				while(p.skipws(false).try_identifier(id).skipws(false).try_one_of_tokens({",",">"}, token)) {
					template_arguments.push_back(expr::make_identifier(id));
					if(token == ">") {
						break;
					}
				}
				if(token != ">") {
					p.raise("expected <arg1, arg2, ...>");
				}

			} else {
				p.back(1);
			}

			if(!p.try_param_list(argsink).try_close_expression()) {
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
					template_arguments,
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
			std::cerr << "ERROR: html/text can not be added to " << current_->sysname() << " node\n";
			throw;
		}

#ifdef PARSER_TRACE
		std::cout << "html: " << html << std::endl;
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
}}
