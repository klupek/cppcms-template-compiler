#ifndef CPPCMS_TEMPLATE_PARSER_V2
#define CPPCMS_TEMPLATE_PARSER_V2

#include <string>
#include <vector>
#include <stack>
#include <stdexcept>
#include <memory>
#include <map>
#include <boost/lexical_cast.hpp>

namespace cppcms { namespace templates {
	struct file_position_t {
		std::string filename;
		size_t line;
	};

	class error_at_line : public std::runtime_error {
		const file_position_t line_;
	public:
		error_at_line(const std::string& msg, const file_position_t& line);
		const file_position_t& line() const;
	};
	
	namespace generator {
		struct context {
			struct view_t { 
				const std::string name, data;
			};
			struct skin_t {
				std::vector< view_t > views;
			};

			std::map<std::string, skin_t> skins;
			std::string current_skin;

			// semi configurables
			std::string variable_prefix;

			// configurables
			std::string skin;
		};
	}


	namespace expr {    
		class base_t;	
		class number_t;
		class variable_t;
		class string_t;
		class filter_t;
		class name_t;
		class text_t;
		class html_t;
		class xhtml_t;
		class identifier_t;
		class call_list_t;
		class param_list_t;
		class cpp_t;

		typedef std::shared_ptr<base_t> ptr;
		typedef std::shared_ptr<number_t> number;
		typedef std::shared_ptr<variable_t> variable;
		typedef std::shared_ptr<filter_t> filter;
		typedef std::shared_ptr<string_t> string;
		typedef std::shared_ptr<name_t> name;
		typedef std::shared_ptr<identifier_t> identifier;
		typedef std::shared_ptr<call_list_t> call_list;
		typedef std::shared_ptr<param_list_t> param_list;
		typedef std::shared_ptr<cpp_t> cpp;
		typedef std::shared_ptr<text_t> text;
		typedef std::shared_ptr<html_t> html;
		typedef std::shared_ptr<xhtml_t> xhtml;

		number make_number(const std::string& repr);
		variable make_variable(const std::string& repr);
		filter make_filter(const std::string& repr);
		string make_string(const std::string& repr);
		name make_name(const std::string& repr);
		identifier make_identifier(const std::string& repr);
		call_list make_call_list(const std::string& repr);
		param_list make_param_list(const std::string& repr);
		cpp make_cpp(const std::string& repr);
		text make_text(const std::string& repr);
		html make_html(const std::string& repr);
		xhtml make_xhtml(const std::string& repr);
		
		class base_t {
		protected:
			const std::string value_;
		public:
			explicit base_t(const std::string& value);

			template<typename T>
			bool is_a() const { return dynamic_cast<const T*>(this) != nullptr; }

			template<typename T>
			T& as() { return dynamic_cast<T&>(*this); }
			
			template<typename T>
			const T& as() const { return dynamic_cast<const T&>(*this); }

			virtual std::string repr() const = 0;
			virtual ~base_t() {}
		};

		class text_t : public base_t {
		public:
			using base_t::base_t;
			virtual std::string repr() const;
		};

		class html_t : public text_t { using text_t::text_t; };
		class xhtml_t : public text_t { using text_t::text_t; };

		class number_t : public base_t {
		public:
			using base_t::base_t;
			double real() const;
			int integer() const;
			virtual std::string repr() const;
		};

		class variable_t : public base_t {
		public:
			using base_t::base_t;
			virtual std::string repr() const;
			std::string code(generator::context&) const;
		};
		
		class string_t : public base_t {
		public:
			string_t(const std::string&);
			std::string repr() const;
			virtual std::string unescaped() const;
		};

		class name_t : public base_t {
		public:
			using base_t::base_t;
			bool operator<(const name_t& rhs) const;
			std::string repr() const;
		};
		
		class identifier_t : public base_t {
		public:
			using base_t::base_t;
			std::string repr() const;
		};
		
		class call_list_t : public base_t {
			const std::vector<ptr> arguments_;
		public:
			call_list_t(const std::string& expr); 
			std::string repr() const;
			std::string code(generator::context& context, const std::string& function_prefix = std::string(), const std::string argument = std::string()) const;
		};
		
		class param_list_t : public base_t {
		public:
			param_list_t(const std::string&);
			using base_t::base_t;
			std::string repr() const;
		};
		
		class filter_t : public call_list_t {
			bool exp_;
		public:
			using call_list_t::call_list_t;
			filter_t(const std::string&);		
			bool is_exp() const;
			std::string code(generator::context& context, const std::string argument) const;
		};

		class cpp_t : public base_t { 
		public:
			using base_t::base_t;
			std::string repr() const;
		};


		std::ostream& operator<<(std::ostream& o, const name_t& obj);
		std::ostream& operator<<(std::ostream& o, const identifier_t& obj);
		std::ostream& operator<<(std::ostream& o, const string_t& obj);
		std::ostream& operator<<(std::ostream& o, const param_list_t& obj);
		std::ostream& operator<<(std::ostream& o, const call_list_t& obj);
		std::ostream& operator<<(std::ostream& o, const filter_t& obj);
		std::ostream& operator<<(std::ostream& o, const variable_t& obj);
		std::ostream& operator<<(std::ostream& o, const cpp_t& obj);
		std::ostream& operator<<(std::ostream& o, const text_t& obj);
		std::ostream& operator<<(std::ostream& o, const html_t& obj);
		std::ostream& operator<<(std::ostream& o, const xhtml_t& obj);
		std::ostream& operator<<(std::ostream& o, const base_t& obj);		
	}
	
	namespace ast {

		class base_t;
		class view_t;
		class root_t;
		class template_t;

		typedef std::shared_ptr<base_t> base_ptr;
		typedef std::shared_ptr<view_t> view_ptr;
		typedef std::shared_ptr<root_t> root_ptr;
		typedef std::shared_ptr<template_t> template_ptr;
		
		typedef base_t& base_ref;
		typedef view_t& view_ref;
		typedef root_t& root_ref;
		typedef template_t& template_ref;	

		class base_t : public std::enable_shared_from_this<base_t> {
			std::string sysname_;
			base_ptr parent_;
			bool block_;
			file_position_t line_;
		protected:
			base_t(const std::string& sysname, file_position_t line, bool block, base_ptr parent);

		public:
			file_position_t line() const;
			virtual ~base_t() {}
			template<typename T>
			bool is_a() const { return dynamic_cast<const T*>(this) != nullptr; }

			// TODO: verbose errors instead of bad_cast
			template<typename T>
			T& as() { 
				try {
					return dynamic_cast<T&>(*this); 
				} catch(const std::bad_cast&) {
					std::cerr << "bad dynamic cast from " << typeid(*this).name() << " to " << typeid(T()).name() << std::endl;
					throw;
				}
			}

			virtual void write(generator::context& context, std::ostream&) = 0;
			virtual void dump(std::ostream& o, int tabs = 0) const = 0;
			virtual base_ptr end(const std::string& what, file_position_t line) = 0;
			base_ptr parent();
			const std::string& sysname() const;
			bool block() const;
		};
		
		class text_t : public base_t {
			const expr::ptr value_;
		public:
			text_t(const expr::ptr& value, file_position_t line, base_ptr parent);
			virtual void write(generator::context& context, std::ostream&);
			virtual void dump(std::ostream& o, int tabs = 0) const;
			virtual base_ptr end(const std::string& what, file_position_t line);
		};


		class root_t : public base_t {
			struct code_t {
				file_position_t line;
				expr::cpp code;
			};
			std::vector<code_t> codes;
			typedef std::map< expr::name_t, view_ptr > view_set_t;
			struct skin_t {
				file_position_t line, endline;
				view_set_t views;
			};

			typedef std::map< expr::name_t, skin_t > skins_t;
			skins_t skins;
			skins_t::iterator current_skin;
			std::string mode_;
			file_position_t mode_line_;
		public:
			root_t();
			base_ptr add_skin(const expr::name& name, file_position_t line);
			base_ptr set_mode(const std::string& mode, file_position_t line);
			base_ptr add_cpp(const expr::cpp& code, file_position_t line);
			base_ptr add_view(const expr::name& name, file_position_t line, const expr::identifier& data, const expr::name& parent);
			std::string mode() const;
			virtual void dump(std::ostream& o, int tabs = 0) const;
			virtual void write(generator::context& context, std::ostream& o);
			virtual base_ptr end(const std::string& what, file_position_t line);
			// TODO: add .clear() method, circular dependencies in shared pointers
		};	
		
		class view_t : public base_t {
			typedef std::vector<std::pair<expr::name_t, template_ptr>> templates_t;
			templates_t templates;
			const expr::name name_, master_;
			const expr::identifier data_;
			file_position_t endline_;
		public:
			base_ptr add_template(const expr::name& name, file_position_t line, const expr::param_list& arguments);
			virtual void dump(std::ostream& o, int tabs = 0) const;
			view_t(const expr::name& name, file_position_t line, const expr::identifier& data, const expr::name& master, base_ptr parent);
			virtual void write(generator::context& context, std::ostream& o);
			virtual base_ptr end(const std::string& what, file_position_t line);
		};
		
		class has_children : public base_t {
		protected:
			std::vector<base_ptr> children;
			file_position_t endline_;
		public:
			has_children(const std::string& sysname, file_position_t line, bool block, base_ptr parent);

			template<typename T, typename... Args>
			base_ptr add(Args&&... args) { 
				children.emplace_back(
					std::make_shared<T>(
						std::forward<Args>(args)..., 
						shared_from_this()
					)
				); 
				if(children.back()->block())
					return children.back();
				else
					return shared_from_this();
			}
			
			virtual void dump(std::ostream& o, int tabs = 0) const;
			virtual void write(generator::context& context, std::ostream& o);
		};

		class template_t : public has_children {
			const expr::name name_;
			const expr::param_list arguments_;
		public:
			template_t(const expr::name& name, file_position_t line, const expr::param_list& arguments, base_ptr parent);
			virtual void dump(std::ostream& o, int tabs = 0) const;
			virtual void write(generator::context& context, std::ostream& o);
			virtual base_ptr end(const std::string& what, file_position_t line);
			
		};


		class cppcode_t : public base_t {
			const expr::cpp code_;	
		public:
			cppcode_t(const expr::cpp& code_, file_position_t line, base_ptr parent);
			virtual void dump(std::ostream& o, int tabs = 0) const;
			virtual void write(generator::context& context, std::ostream& o);
			virtual base_ptr end(const std::string& what, file_position_t line);
		};

		class variable_t : public base_t {
			const expr::variable name_;
			const std::vector<expr::filter> filters_;
		public:
			variable_t(const expr::variable& name, file_position_t line, const std::vector<expr::filter>& filters, base_ptr parent);
			virtual void dump(std::ostream& o, int tabs = 0) const;
			virtual void write(generator::context& context, std::ostream& o);
			std::string code(generator::context& context, const std::string& escaper = "cppcms::filters::escape") const;
			virtual base_ptr end(const std::string& what, file_position_t line);
		};
			
		typedef variable_t using_option_t;
		typedef std::vector<using_option_t> using_options_t;

		class fmt_function_t : public base_t {
		protected:
			const std::string name_;
		        const expr::string fmt_;
			const using_options_t using_options_;
		public:
			fmt_function_t(const std::string& name, file_position_t line, const expr::string& fmt, 
					const using_options_t& uos, base_ptr parent);
			virtual void dump(std::ostream& o, int tabs = 0) const;
			virtual void write(generator::context& context, std::ostream& o);
			virtual base_ptr end(const std::string& what, file_position_t line);
		};

		class ngt_t : public base_t {
			const expr::string singular_, plural_;
			const expr::variable variable_;
			const using_options_t using_options_;
		public:
			ngt_t(file_position_t line, const expr::string& singular, const expr::string& plural, const expr::variable& variable, const using_options_t& uos, base_ptr parent);
			virtual void dump(std::ostream& o, int tabs = 0) const;
			virtual void write(generator::context& context, std::ostream& o);
			virtual base_ptr end(const std::string& what, file_position_t line);
		};

		class include_t : public base_t {
			const expr::call_list name_;
			const expr::identifier from_, using_;
			const expr::variable with_;
			const std::vector<std::string> arguments_;
		public:
			include_t(const expr::call_list& name, file_position_t line, const expr::identifier& from, 
					const expr::identifier& _using, const expr::variable& with, base_ptr parent);
			virtual void dump(std::ostream& o, int tabs = 0) const;
			virtual void write(generator::context& context, std::ostream& o);
			virtual base_ptr end(const std::string& what, file_position_t line);
		};

		class form_t : public base_t {
			const expr::name style_;
			const expr::variable name_;
		public:
			form_t(const expr::name& style, file_position_t line, const expr::variable& name, base_ptr parent);
			virtual void dump(std::ostream& o, int tabs = 0) const;
			virtual void write(generator::context& context, std::ostream& o);
			virtual base_ptr end(const std::string& what, file_position_t line);
		};
		
		class csrf_t : public base_t {
			const expr::name style_;
		public:
			csrf_t(file_position_t line, const expr::name& style, base_ptr parent);
			virtual void dump(std::ostream& o, int tabs = 0) const;
			virtual void write(generator::context& context, std::ostream& o);
			virtual base_ptr end(const std::string& what, file_position_t line);
		};

		class render_t : public base_t {
			const expr::ptr skin_, view_;
			const expr::variable with_;
		public:
			render_t(file_position_t line, const expr::ptr& skin, const expr::ptr& view, const expr::variable& with, base_ptr parent);
			virtual void dump(std::ostream& o, int tabs = 0) const;
			virtual void write(generator::context& context, std::ostream& o);
			virtual base_ptr end(const std::string& what, file_position_t line);
		};

		class using_t : public has_children {
			const expr::identifier id_;
			const expr::variable with_;
			const expr::identifier as_;
		public:
			using_t(file_position_t line, const expr::identifier& id, const expr::variable& with, const expr::identifier& as, base_ptr parent);
			virtual void dump(std::ostream& o, int tabs = 0) const;
			virtual void write(generator::context& context, std::ostream& o);
			virtual base_ptr end(const std::string& what, file_position_t line);
		};

		class if_t : public has_children {
		public:	
			enum type_t { if_regular, if_empty, if_rtl, if_cpp, if_else };
		private:
			class condition_t : public has_children {
				const type_t type_;
				const expr::cpp cond_;
				const expr::variable variable_;
				const bool negate_;
			public:
				condition_t(file_position_t line, type_t type, const expr::cpp& cond, const expr::variable& variable, bool negate, base_ptr parent);
				virtual void dump(std::ostream& o, int tabs = 0) const;
				virtual void write(generator::context& context, std::ostream& o);
				virtual base_ptr end(const std::string& what, file_position_t line);
			};
			typedef std::shared_ptr<condition_t> condition_ptr;
			std::vector< condition_ptr > conditions_;
		public:
			if_t(file_position_t line, base_ptr parent);
			
			base_ptr add_condition(file_position_t line, type_t type, bool negate);
			base_ptr add_condition(file_position_t line, const type_t& type, const expr::variable& variable, bool negate);
			base_ptr add_condition(file_position_t line, const expr::cpp& cond, bool negate);
			
			virtual void dump(std::ostream& o, int tabs = 0) const;
			virtual void write(generator::context& context, std::ostream& o);
			virtual base_ptr end(const std::string& what, file_position_t line);
		};

		class foreach_t : public base_t {
			class part_t : public has_children {
				bool has_end_;
			public:
				part_t(file_position_t line, const std::string& sysname, bool has_end, base_ptr parent);
				virtual base_ptr end(const std::string& what, file_position_t line);
			};
			const expr::name name_;
			const expr::identifier as_;
			const expr::name rowid_; // spec says it is identifier, but it is used as local scope variable name
			const int from_;
			const expr::variable array_;
			const bool reverse_;

			base_ptr empty_, separator_, item_;
			base_ptr item_prefix_, item_suffix_;
		public:
			foreach_t(file_position_t line, const expr::name& name, const expr::identifier& as, const expr::name& rowid, const int from, const expr::variable& array, bool reverse, base_ptr parent);
			virtual void dump(std::ostream& o, int tabs = 0) const;
			virtual void write(generator::context& context, std::ostream& o);
			virtual base_ptr end(const std::string& what, file_position_t line);

			base_ptr prefix(file_position_t line);
			base_ptr empty(file_position_t line);
			base_ptr separator(file_position_t line);
			base_ptr item(file_position_t line);
			base_ptr suffix(file_position_t line);
		};

		class cache_t : public has_children {
			const expr::ptr name_;
			const expr::variable miss_;
			const int duration_;
			const bool recording_, triggers_;
			struct trigger_t {
				file_position_t line;
				expr::ptr ptr;
			};
			std::vector<trigger_t> trigger_list_;
		public:
			cache_t(file_position_t line, const expr::ptr& name, const expr::variable& miss, int duration, bool recording, bool triggers, base_ptr parent);
			base_ptr add_trigger(file_position_t line, const expr::ptr&);
			virtual void dump(std::ostream& o, int tabs = 0) const;
			virtual void write(generator::context& context, std::ostream& o);
			virtual base_ptr end(const std::string& what, file_position_t line);
		};
	}


	class parser_source {
		const std::string input_;
		size_t index_, line_;
		const std::string filename_;
	public:
		parser_source(const std::string& filename);
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
	};

	class parser {
		parser_source source_;
		struct state_t {
			size_t index;
			std::string token;
		};

		std::vector<state_t> stack_;
		std::stack<std::pair<size_t,size_t>> state_stack_;
		// details stack looks like stack or state_stack_, but refactoring it currently would break too many things
	public:
		struct detail_t {
			const std::string what, item;
		};
	private:	
		typedef std::stack<detail_t> details_t;
		details_t details_;

		size_t failed_;
	public:
		file_position_t line() const;
		explicit parser(const std::string& input);

		parser& try_token(const std::string& token);
		parser& try_token_ws(const std::string& token);
		parser& try_token_nl(const std::string& token);


		parser& try_name(); // -> [ NAME ]
		parser& try_name_ws(); // -> [ NAME, \s+ ]
		parser& try_string(); // -> [ STRING, ]
		parser& try_string_ws(); // -> [ STRING, \s+ ]
		parser& try_number(); // -> [ NUMBER ]
		parser& try_number_ws(); // -> [ NUMBER, \s+ ]
		parser& try_variable(); // -> [ VAR ]
		parser& try_variable_ws(); // -> [ VAR, \s+ ]
		parser& try_complex_variable(); // -> [ CVAR ]
		parser& try_complex_variable_ws(); // -> [ CVAR, \s+ ]
		parser& try_filter(); // -> [ FILTER ]
		parser& try_comma(); // [ ',' ]
		parser& try_argument_list(); // [ argument_list ]
		parser& try_identifier(); // -> [ ID ]
		parser& try_identifier_ws(); // -> [ ID, \s+ ]
		parser& skip_to(const std::string& token); // -> [ prefix, token ]
		parser& skipws(bool require); // -> [ \s* ]
		parser& skip_to_end(); // -> [ ... ]
		parser& try_parenthesis_expression(); // [ ... ]
		
		template<typename T=std::string>
		T get(int n);
		bool failed() const;
		bool finished() const;
		operator bool() const;
		parser& back(size_t n);
		void raise(const std::string& msg);
		void raise_at_line(const file_position_t& line, const std::string& msg);

		details_t& details();

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
		ast::root_ptr tree_;
		ast::base_ptr current_;

		ast::using_options_t parse_using_options(std::vector<std::string>&);
	public:
		template_parser(const std::string& input);

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

/*
 * arguments to do: 
 * 	-s (skin name), which replaces __default__ skin name in skin map (compat)
 * 	-m (magic includes, uses data::foobar automagically includes data/foobar.h) (new)
 * 	-std=relaxed (new)
 * 	-std=compat (compat)
 *
 * new keywords:
 * 	pgt
 * 		old: ngt SINGULAR, PLURAL, VARIABLE [ using ... ]
 * 			ngt "You have one apple", "You have {1} apples", apple_count using apple_count
 * 		new: 
 * 			pgt "You have one {1:apple}" using apple_count
 * 			needs creating dictionary, maybe cppcms::template_parser::pluralizer, or in config?
 * 			note: config should contain word => [ [ range, pluralized ] ], so not only english is supported
 * 	form	
 * 		additional form rendering engines should be registerable, 'form' NAME VARIABLE 
 * magic to do:
 * 	collapsing all-whitespace strings in add_html()
 *
 * compat to do:
 * 	if skins.size() != 1 
 * 		throw
 * 	elif !context.skin.empty() skins.first()->name != context.skin
 *  
 *  checks:
 *  	check if include() functions exists
 */

#endif
