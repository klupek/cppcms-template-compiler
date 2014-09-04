#ifndef CPPCMS_TEMPLATE_COMPILER_PARSER_H
#define CPPCMS_TEMPLATE_COMPILER_PARSER_H
#include "parser_source.h"
#include "expr.h"
#include "ast.h"

namespace cppcms { namespace templates {
	class token_sink {		
	public:
		struct detail_t {
			const std::string what, item;
		};

		token_sink(std::string&);
		token_sink();
		void put(const std::string&);
		void add_detail(const std::string& what, const std::string& item);
		bool has_details() const;
		detail_t get_detail();
		const detail_t& top_detail() const;
		const std::string& value() const;
	private:
		std::string tmp_;
		std::string* target_;
		std::shared_ptr<std::stack<detail_t>> details_;
	};

	class parser {
		parser_source source_;

		// index stack, used (only) for back(n), going n tokens up
		struct state_t {
			size_t index;
		};

		std::vector<state_t> stack_;
		std::stack<std::pair<size_t,size_t>> state_stack_;
	public:
		size_t failed_;
	public:
		file_position_t line() const;
		explicit parser(const std::vector<std::string>& files);

		parser& try_token(const std::string& token);
		parser& try_token_ws(const std::string& token);
		parser& try_one_of_tokens(const std::vector<std::string>& tokens, token_sink out = token_sink());


		parser& try_name(token_sink out = token_sink()); // -> [ NAME ]
		parser& try_name_ws(token_sink out = token_sink()); // -> [ NAME, \s+ ]
		parser& try_string(token_sink out = token_sink()); // -> [ STRING, ]
		parser& try_string_ws(token_sink out = token_sink()); // -> [ STRING, \s+ ]
		parser& try_number(token_sink out = token_sink()); // -> [ NUMBER ]
		parser& try_number_ws(token_sink out = token_sink()); // -> [ NUMBER, \s+ ]
		parser& try_variable(token_sink out = token_sink()); // -> [ VAR ]
		parser& try_variable_ws(token_sink out = token_sink()); // -> [ VAR, \s+ ]
		parser& try_complex_variable(token_sink out = token_sink()); // -> [ CVAR ]
		parser& try_complex_variable_ws(token_sink out = token_sink()); // -> [ CVAR, \s+ ]
		parser& try_filter(token_sink out = token_sink()); // -> [ FILTER ]
		parser& try_comma(); // [ ',' ]
		parser& try_argument_list(token_sink out = token_sink()); // [ argument_list ]
		parser& try_param_list(token_sink out = token_sink()); // [ param_list ]
		parser& try_identifier(token_sink out = token_sink()); // -> [ ID ]
		parser& try_identifier_ws(token_sink out = token_sink()); // -> [ ID, \s+ ]
		parser& skip_to(const std::string& token, token_sink out = token_sink()); // -> [ prefix, token ]
		parser& skipws(bool require); // -> [ \s* ]
		parser& skip_to_end(token_sink out = token_sink()); // -> [ ... ]
		parser& try_parenthesis_expression(token_sink out = token_sink()); // [ ... ]

		// input: skip any whitespaces, find '%>' or '% >'
		// stack: add '%>' or '% >'
		parser& try_close_expression(); // %> and variations 
		
		bool failed() const;
		bool finished() const;
		operator bool() const;
		parser& back(size_t n);
		void raise(const std::string& msg);
		void raise_at_line(const file_position_t& line, const std::string& msg);

		// state
		// save current state 
		void push();
		// reset to last saved state
		parser& reset();
		// forget last saved state
		void pop();
		// compress all stack entries between last saved state and current into one entry
		void compress();
	};

	class template_parser {
		parser p;
		ast::root_ptr tree_;
		ast::base_ptr current_;

		ast::using_options_t parse_using_options(std::vector<std::string>&);
	public:
		template_parser(const std::vector<std::string>& files);

		void parse();

		ast::root_ptr tree();
		void write(generator::context& context, std::ostream& o);
	private:
		bool try_flow_expression();
		bool try_global_expression();
		bool try_render_expression();
		bool try_variable_expression();
		
		// actions
		void add_html(const std::string&);
		void add_cpp(const expr::cpp&);

	};
}}
#endif
