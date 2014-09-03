#ifndef CPPCMS_TEMPLATE_PARSER_V2
#define CPPCMS_TEMPLATE_PARSER_V2

#include <string>
#include <vector>
#include <stack>
#include <stdexcept>
#include <memory>
#include <map>
#include <set>
#include <list>
#include <boost/lexical_cast.hpp>

// for demangle only
#include <cxxabi.h>
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

	// FIXME: make better (aka: copy of boost demangle) wrapper, or use newer (1.56) boost
	inline std::string demangle(const char* name) {
		std::size_t size = 0;
		int status = 0;
		char *demangled = abi::__cxa_demangle( name, NULL, &size, &status );
		return demangled ? std::string(demangled) : ( std::string("[demangle failed:") + name + "]");
	}

	std::string translate_ast_object_name(const std::string& name); 

	class bad_cast : public std::runtime_error {
	public:
		using std::runtime_error::runtime_error;
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

			// runtime state (with loadable defaults)
			typedef std::string include_t;
			std::set<include_t> includes;
			std::string variable_prefix;
			std::string output_mode;
			void add_scope_variable(const std::string& name);
			void remove_scope_variable(const std::string& name);
			bool check_scope_variable(const std::string& name);
			void add_include(const std::string& include);

			// configurables
			std::string skin;
			enum {
				// levels
				compat 				= 0x0,
				magic1				= compat
			};
			int level;

		private:
			std::set<std::string> scope_variables;

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
			virtual std::string code(generator::context&) const = 0;
			virtual ~base_t() {}
		};

		class text_t : public base_t {
		public:
			using base_t::base_t;
			virtual std::string repr() const;
			virtual std::string code(generator::context&) const;
		};

		class html_t : public text_t { using text_t::text_t; };
		class xhtml_t : public text_t { using text_t::text_t; };

		class number_t : public base_t {
		public:
			using base_t::base_t;
			double real() const;
			int integer() const;
			virtual std::string repr() const;
			virtual std::string code(generator::context&) const;
		};

		class variable_t : public base_t {
			struct part_t {
				const std::string name;
				const std::vector<ptr> arguments;
				const std::string separator;
				const bool is_function;
			};
			bool is_deref;
			std::vector<part_t> parts;
		public:
			variable_t(const std::string&, bool consume_all = true,  size_t* pos = nullptr);
			
			virtual std::string repr() const;
			virtual std::string code(generator::context&) const;
		private:
			std::vector<ptr> parse_arguments(const std::string&, size_t&);			
			ptr parse_string(const std::string&, size_t&);	
			ptr parse_number(const std::string&, size_t&);	
		};
		
		class string_t : public base_t {
		public:
			string_t(const std::string&);
			std::string repr() const;
			virtual std::string unescaped() const;
			virtual std::string code(generator::context&) const;
		};

		class name_t : public base_t {
		public:
			using base_t::base_t;
			bool operator<(const name_t& rhs) const;
			std::string repr() const;
			virtual std::string code(generator::context&) const;
		};
		
		class identifier_t : public base_t {
		public:
			using base_t::base_t;
			std::string repr() const;
			virtual std::string code(generator::context&) const;
		};
		
		class call_list_t : public base_t {
			const std::vector<ptr> arguments_;
			const std::string function_prefix_;
			std::string current_argument_;
		public:
			call_list_t(const std::string& expr, const std::string& function_prefix); 
			call_list_t& argument(const std::string&);
			std::string repr() const;
			virtual std::string code(generator::context& context) const;
		};
		
		class param_list_t : public base_t {
		public:
			struct param_t {
				expr::identifier type;
				bool is_const, is_ref;
				expr::name name;
			};
			typedef std::vector<param_t> params_t;

			param_list_t(const std::string&, const params_t&);
			using base_t::base_t;
			std::string repr() const;
			const params_t& params() const;
						
			virtual std::string code(generator::context& context) const;
		private:
			const params_t params_;
		};
		
		class filter_t : public call_list_t {
			const bool exp_;
			const std::string current_argument_;
		public:
			using call_list_t::call_list_t;
			filter_t(const std::string&);		
			bool is_exp() const;
			virtual std::string code(generator::context& context) const;
		};

		class cpp_t : public base_t { 
		public:
			using base_t::base_t;
			std::string repr() const;
			virtual std::string code(generator::context& context) const;
		};
		
		number make_number(const std::string& repr);
		variable make_variable(const std::string& repr);
		filter make_filter(const std::string& repr);
		string make_string(const std::string& repr);
		name make_name(const std::string& repr);
		identifier make_identifier(const std::string& repr);
		call_list make_call_list(const std::string& repr, const std::string& prefix);
		param_list make_param_list(const std::string& repr, const param_list_t::params_t&);
		cpp make_cpp(const std::string& repr);
		text make_text(const std::string& repr);
		html make_html(const std::string& repr);
		xhtml make_xhtml(const std::string& repr);
		


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
		class has_children;
		class form_theme_t;

		typedef std::shared_ptr<base_t> base_ptr;
		typedef std::shared_ptr<view_t> view_ptr;
		typedef std::shared_ptr<root_t> root_ptr;
		typedef std::shared_ptr<template_t> template_ptr;
		typedef std::shared_ptr<has_children> has_children_ptr;
		typedef std::shared_ptr<form_theme_t> form_theme_ptr;
		
		typedef base_t& base_ref;
		typedef view_t& view_ref;
		typedef root_t& root_ref;		
		typedef template_t& template_ref;	
		typedef form_theme_t& form_theme_ref;

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
					std::string tgt = demangle(typeid(T).name());
					std::string src = demangle(typeid(*this).name());
					// make some translations
					const std::string msg = std::string("could not insert child node: parent node is ") 
						+ translate_ast_object_name(src)
					        + ", but it should be "
						+ translate_ast_object_name(tgt);
					throw bad_cast(msg);
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

			typedef std::list< std::pair< expr::name_t, view_ptr> > view_set_t;
			struct skin_t {
				file_position_t line, endline;
				view_set_t views;
			};

			typedef std::list< std::pair<expr::name_t, skin_t> > skins_t;
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
			base_ptr add_form_theme(const expr::name& name, file_position_t line, const expr::name& parent);
			std::string mode() const;
			virtual void dump(std::ostream& o, int tabs = 0) const;
			virtual void write(generator::context& context, std::ostream& o);
			virtual base_ptr end(const std::string& what, file_position_t line);
			// TODO: add .clear() method, circular dependencies in shared pointers
		};	
		
		class view_t : public base_t {
		protected:
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

		class form_theme_t : public view_t {
		public:
			virtual void dump(std::ostream& o, int tabs = 0) const;
			form_theme_t(const expr::name& name, file_position_t line, const expr::name& master, base_ptr parent);
			void init();
			virtual base_ptr end(const std::string& what, file_position_t line);
		};
		
		class has_children : public base_t {
		protected:
			std::vector<base_ptr> children;
			file_position_t endline_;
		public:
			has_children(const std::string& sysname, file_position_t line, bool block, base_ptr parent);
			file_position_t endline() const;

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

		class form_t : public has_children {
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
			enum class type_t { if_regular, if_empty, if_rtl, if_cpp, if_else };
			class condition_t : public has_children {
				const type_t type_;
				const expr::cpp cond_;
				const expr::variable variable_;
				const bool negate_;
			public:
				enum class next_op_t { op_or, op_and };
			private:
				std::vector<std::pair<std::shared_ptr<condition_t>, next_op_t>> next;
			public:
				condition_t(file_position_t line, type_t type, const expr::cpp& cond, const expr::variable& variable, bool negate, base_ptr parent);
				void add_next(const next_op_t& no, const type_t& type, const expr::variable& variable, bool negate);
				type_t type() const;
				virtual void dump(std::ostream& o, int tabs = 0) const;
				virtual void write(generator::context& context, std::ostream& o);
				virtual base_ptr end(const std::string& what, file_position_t line);
			};
			typedef std::shared_ptr<condition_t> condition_ptr;
		private:
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

			has_children_ptr empty_, separator_, item_;
			has_children_ptr item_prefix_, item_suffix_;
		public:
			foreach_t(file_position_t line, const expr::name& name, const expr::identifier& as, const expr::name& rowid, const int from, const expr::variable& array, bool reverse, base_ptr parent);
			virtual void dump(std::ostream& o, int tabs = 0) const;
			virtual void write(generator::context& context, std::ostream& o);
			virtual base_ptr end(const std::string& what, file_position_t line);

			has_children_ptr prefix(file_position_t line);
			has_children_ptr empty(file_position_t line);
			has_children_ptr separator(file_position_t line);
			has_children_ptr item(file_position_t line);
			has_children_ptr suffix(file_position_t line);
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

	struct file_index_t {
		const std::string filename;
		const size_t beg, end;
		const size_t line_beg, line_end;
	};

	class parser_source {
		const std::pair<std::string, std::vector<file_index_t>> input_pair_;
		const std::string input_;
		size_t index_, line_;
		std::vector<file_index_t> file_indexes_;
		std::stack< size_t > marks_;
	public:
		parser_source(const std::vector<std::string>& files);
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

		void mark();
		void unmark();
		size_t get_mark() const;
		std::string right_from_mark();
	};

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
 * 	namespace cppcms { namespace views { struct display_traits<mytype> { ...
 * 	collapsing all-whitespace strings in add_html()
 * 	form templates
 * 		skin, view, blah, blah, 
 * 		<% template (text,password,radio,checkbox,whatever)(all input options, including value, classes etc) %>
 *
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
