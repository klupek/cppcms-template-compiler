#include "parser.h"
namespace cppcms { namespace templates {
	parser::parser(const std::string& input)
		: input_(input)
		, index_(0)
       		, failed_(0) {}
	
	parser& parser::try_token(const std::string& token) {
		if(!failed_ && input_.length() >= token.length() && input_.compare(index_, token.length(), token) == 0) {
			stack_.emplace_back(state_t { index_, token });
			index_ += token.length();
		} else {
			failed_ ++;
		}
		return *this;
	}

	void parser::raise(const std::string& msg) {
		const int context = 10;
		const std::string left = ( index_ >= context ? input_.substr(index_ - context, context) : input_.substr(0, index_) );
		const std::string right = ( index_ + context < input_.size() ? input_.substr(index_+1, context) : input_.substr(index_) );
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

	void parser::back(size_t n) {
		if(n > failed_ + stack_.size())
			throw std::logic_error("Attempt to clear more tokens then stack_.size()+failed_");

		n -= failed_;
		failed_ = 0;
		
		while(n-- > 0) {
			index_ = stack_.back().index;
			stack_.pop_back();
		}
	}

	parser& parser::skip_to(const std::string& token) {
		if(!failed_) {
			size_t r = input_.find(token, index_);
			if(r == std::string::npos) {
				failed_ ++;
			} else {
				stack_.emplace_back(state_t { index_, input_.substr(index_, r - index_) } );
				stack_.emplace_back(state_t { r, token });
				index_ = r + token.length();
			}
		}
		return *this;
	}

	parser& parser::skipws(bool require) {
		if(!failed_) {
			const size_t start = index_;
			for(;index_ < input_.length() && std::isspace(input_[index_]); ++index_);
			stack_.emplace_back( state_t { start, input_.substr(start, index_ - start) });

			if(require && index_ == start)
				failed_++;
		}
		return *this;
	}
	
	parser& parser::try_token_ws(const std::string& token) {
		try_token(token);
		skipws(true);
		return *this;
	}

	parser& parser::skip_to_end() {
		if(!failed_) {
			stack_.emplace_back( state_t { index_, input_.substr(index_) });
			index_ = input_.length();
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
		: p(input) {}
		

	void template_parser::parse() {
		while(!p.finished() && !p.failed()) {
			std::cout << "loop\n";
			p.push();
			if(p.skip_to("<%=").skipws(true)) { // [ <blah><blah>..., <%=, \s+ ] = 3
				add_html(p.get(-3));
				if(!try_variable_expression()) {
					p.raise("expected variable expression");
				}
			} else if(p.reset().skip_to("<%").skipws(true)) { // [ <blah><blah>..., <%, \s+ ] = 3
				add_html(p.get(-3));
				if(!try_flow_expression() && !try_global_expression() && !try_render_expression()) {
					p.raise("expected c++, global, render or flow expression");
				}
			} else if(p.reset().skip_to_end()) { // [ <blah><blah>EOF ]
				add_html(p.get(-1));
			} else {
				p.reset().raise("expected <%=, <% or EOF");
			}
			p.pop();
		}
	}

	parser& template_parser::try_flow_expression() {
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
			std::cout << "flow: end\n";
		} else if(p.reset().try_token_ws("cache")) {
			std::cout << "flow: cache\n";
		} else
			p.reset();
		p.pop();
		return p;
	}

	parser& template_parser::try_global_expression() {
		p.push();

		if(p.try_token_ws("skin")) {
			std::cout << "global: skin\n";
		} else if(p.reset().try_token_ws("view")) {
			std::cout << "global: view\n";
		} else if(p.reset().try_token_ws("template")) {
			std::cout << "global: template\n";
		} else if(p.reset().try_token_ws("c++").skip_to("%>")) { // [ c++, \s+, cppcode, %> ] = 4
		} else {
			p.reset();
		}
		p.pop();
		return p;
	}

	parser& template_parser::try_render_expression() {
		p.push();
		if(p.try_token_ws("gt")) {
			std::cout << "render: gt\n";
		} else if(p.reset().try_token_ws("ngt")) {
			std::cout << "render: ngt\n";
		} else if(p.reset().try_token_ws("url")) {
			std::cout << "render: url\n";
		} else if(p.reset().try_token_ws("include")) {
			std::cout << "render: include\n";
		} else if(p.reset().try_token_ws("form")) {
			std::cout << "render: form\n";
		} else if(p.reset().try_token_ws("csrf")) {
			std::cout << "render: csrf\n";
		} else
			p.reset();
		p.pop();
		return p;
	}

	parser& template_parser::try_variable_expression() {
		p.push();
		if(p.skip_to("%>")) { // [ variable xpr, %> ]
			const std::string expr = p.get<std::string>(-2); 
			std::cout << "variable: " << expr << std::endl;
		} else
			p.reset();
		p.pop();
		return p;
	}

	void template_parser::add_html(const std::string& html) {
		std::cout << "html: " << html << std::endl;
	}

	void template_parser::add_cpp(const std::string& cpp) {
		std::cout << "cpp: " << cpp << std::endl;
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
