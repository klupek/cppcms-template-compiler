#ifndef CPPCMS_TEMPLATE_PARSER_V2
#define CPPCMS_TEMPLATE_PARSER_V2

#include <string>
#include <vector>
#include <stack>
#include <stdexcept>
#include <boost/lexical_cast.hpp>

namespace cppcms { namespace templates {
	class parser {
		std::string input_;
		struct state_t {
			size_t index;
			std::string token;
		};

		std::vector<state_t> stack_;
		std::stack<std::pair<size_t,size_t>> state_stack_;
		size_t index_;
		size_t failed_;
	public:
		explicit parser(const std::string& input);
		parser& try_token(const std::string& token);
		parser& try_token_ws(const std::string& token);
		parser& skip_to(const std::string& token);
		parser& skipws(bool require);
		parser& skip_to_end();
		
		template<typename T=std::string>
		T get(int n);
		bool failed() const;
		bool finished() const;
		operator bool() const;
		void back(size_t n);
		void raise(const std::string& msg);

		// state
		void push();
		parser& reset();
		void pop();
	};

	template<typename T>
	T parser::get(int n) {
		if(failed_) {
			throw std::logic_error("Attempt to get value from failed parser");
		} else if(static_cast<size_t>(-n) > stack_.size()) {
			throw std::logic_error("Value index too large");
		} else if(n >= 0) {
			throw std::logic_error("get(n>=0) is invalid");
		} else {
			return boost::lexical_cast<T>(stack_[stack_.size()+n].token);
		}
	}

	class template_parser {
		parser p;
	public:
		template_parser(const std::string& input);
		parser& try_flow_expression();
		parser& try_global_expression();
		parser& try_render_expression();
		parser& try_variable_expression();

		void parse();

		// actions
		void add_html(const std::string&);
		void add_cpp(const std::string&);
	};
}}

#endif
