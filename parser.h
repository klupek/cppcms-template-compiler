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
	namespace expr {    
		class base_t;	
		class number_t;
		class variable_t;
		class string_t;
		class filter_t;
		class name_t;
		class identifier_t;
		class call_list_t;
		class param_list_t;

		typedef std::shared_ptr<base_t> ptr;
		typedef std::shared_ptr<number_t> number;
		typedef std::shared_ptr<variable_t> variable;
		typedef std::shared_ptr<filter_t> filter;
		typedef std::shared_ptr<string_t> string;
		typedef std::shared_ptr<name_t> name;
		typedef std::shared_ptr<identifier_t> identifier;
		typedef std::shared_ptr<call_list_t> call_list;
		typedef std::shared_ptr<param_list_t> param_list;

		number make_number(const std::string& repr);
		variable make_variable(const std::string& repr);
		filter make_filter(const std::string& repr);
		string make_string(const std::string& repr);
		name make_name(const std::string& repr);
		identifier make_identifier(const std::string& repr);
		call_list make_call_list(const std::string& repr);
		param_list make_param_list(const std::string& repr);
		
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
		};
		
		class string_t : public base_t {
		public:
			using base_t::base_t;
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
		};
		
		class param_list_t : public base_t {
		public:
			param_list_t(const std::string&);
			using base_t::base_t;
			std::string repr() const;
		};
		
		class filter_t : public call_list_t {
		public:
			using call_list_t::call_list_t;
		};


		std::ostream& operator<<(std::ostream& o, const name_t& obj);
		std::ostream& operator<<(std::ostream& o, const identifier_t& obj);
		std::ostream& operator<<(std::ostream& o, const string_t& obj);
		std::ostream& operator<<(std::ostream& o, const param_list_t& obj);
		std::ostream& operator<<(std::ostream& o, const call_list_t& obj);
		std::ostream& operator<<(std::ostream& o, const filter_t& obj);
		std::ostream& operator<<(std::ostream& o, const variable_t& obj);
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
		protected:
			base_t(const std::string& sysname, bool block, base_ptr parent);

		public:
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

			virtual void write(std::ostream&) = 0;
			virtual void dump(std::ostream& o, int tabs = 0) = 0;
			virtual base_ptr end(const std::string& what) = 0;
			base_ptr parent();
			const std::string& sysname() const;
			bool block() const;
		};

		class root_t : public base_t {
			std::vector<std::string> codes;
			typedef std::map< expr::name_t, view_ptr > view_set_t;
			typedef std::map< expr::name_t, view_set_t > skins_t;
			skins_t skins;
			skins_t::iterator current_skin;
			std::string mode_;
		public:
			root_t();
			base_ptr add_skin(const expr::name& name);
			base_ptr set_mode(const std::string& mode);
			base_ptr add_cpp(const std::string& code);
			base_ptr add_view(const expr::name& name, const expr::identifier& data, const expr::name& parent);
			virtual void dump(std::ostream& o, int tabs = 0);
			virtual void write(std::ostream& o);
			virtual base_ptr end(const std::string& what);
			// TODO: add .clear() method, circular dependencies in shared pointers
		};	
		
		class view_t : public base_t {
			typedef std::map<expr::name_t, template_ptr> templates_t;
			templates_t templates;
			const expr::name name_, master_;
			const expr::identifier data_;
		public:
			base_ptr add_template(const expr::name& name, const expr::param_list& arguments);
			virtual void dump(std::ostream& o, int tabs = 0);
			view_t(const expr::name& name, const expr::identifier& data, const expr::name& master, base_ptr parent);
			virtual void write(std::ostream& o);
			virtual base_ptr end(const std::string& what);
		};
		
		class has_children : public base_t {
		protected:
			std::vector<base_ptr> children;
		public:
			has_children(const std::string& sysname, bool block, base_ptr parent);

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
			
			virtual void dump(std::ostream& o, int tabs = 0);
			virtual void write(std::ostream& o);
		};

		class template_t : public has_children {
			const expr::name name_;
			const expr::param_list arguments_;
		public:
			template_t(const expr::name& name, const expr::param_list& arguments, base_ptr parent);
			virtual void dump(std::ostream& o, int tabs = 0);
			virtual void write(std::ostream& o);
			virtual base_ptr end(const std::string& what);
			
		};


		class cppcode_t : public base_t {
			const std::string code_;	
		public:
			cppcode_t(const std::string& code_, base_ptr parent);
			virtual void dump(std::ostream& o, int tabs = 0);
			virtual void write(std::ostream& o);
			virtual base_ptr end(const std::string& what);
		};

		class variable_t : public base_t {
			const expr::variable name_;
			const std::vector<expr::filter> filters_;
		public:
			variable_t(const expr::variable& name, const std::vector<expr::filter>& filters, base_ptr parent);
			virtual void dump(std::ostream& o, int tabs = 0);
			virtual void write(std::ostream& o);
			virtual base_ptr end(const std::string& what);
		};
			
		struct using_option_t {
			const expr::variable variable;
			const std::vector<expr::filter> filters;
		};
		typedef std::vector<using_option_t> using_options_t;

		class fmt_function_t : public base_t {
		protected:
			const std::string name_;
		        const expr::string fmt_;
			const using_options_t using_options_;
		public:
			fmt_function_t(const std::string& name, const expr::string& fmt, 
					const using_options_t& uos, base_ptr parent);
			virtual void dump(std::ostream& o, int tabs = 0);
			virtual void write(std::ostream& o);
			virtual base_ptr end(const std::string& what);
		};

		class ngt_t : public base_t {
			const expr::string singular_, plural_;
			const expr::variable variable_;
			const using_options_t using_options_;
		public:
			ngt_t(const expr::string& singular, const expr::string& plural, const expr::variable& variable, const using_options_t& uos, base_ptr parent);
			virtual void dump(std::ostream& o, int tabs = 0);
			virtual void write(std::ostream& o);
			virtual base_ptr end(const std::string& what);
		};

		class include_t : public base_t {
			const expr::call_list name_;
			const expr::identifier from_, using_;
			const expr::variable with_;
			const std::vector<std::string> arguments_;
		public:
			include_t(const expr::call_list& name, const expr::identifier& from, 
					const expr::identifier& _using, const expr::variable& with, base_ptr parent);
			virtual void dump(std::ostream& o, int tabs = 0);
			virtual void write(std::ostream& o);
			virtual base_ptr end(const std::string& what);
		};

		class form_t : public base_t {
			const expr::name style_;
			const expr::variable name_;
		public:
			form_t(const expr::name& style, const expr::variable& name, base_ptr parent);
			virtual void dump(std::ostream& o, int tabs = 0);
			virtual void write(std::ostream& o);
			virtual base_ptr end(const std::string& what);
		};
		
		class csrf_t : public base_t {
			const expr::name style_;
		public:
			csrf_t(const expr::name& style, base_ptr parent);
			virtual void dump(std::ostream& o, int tabs = 0);
			virtual void write(std::ostream& o);
			virtual base_ptr end(const std::string& what);
		};

		class render_t : public base_t {
			const expr::ptr skin_, view_;
			const expr::variable with_;
		public:
			render_t(const expr::ptr& skin, const expr::ptr& view, const expr::variable& with, base_ptr parent);
			virtual void dump(std::ostream& o, int tabs = 0);
			virtual void write(std::ostream& o);
			virtual base_ptr end(const std::string& what);
		};

		class using_t : public has_children {
			const std::string id_, with_, as_;
		public:
			using_t(const std::string& id, const std::string& with, const std::string& as, base_ptr parent);
			virtual void dump(std::ostream& o, int tabs = 0);
			virtual void write(std::ostream& o);
			virtual base_ptr end(const std::string& what);
		};

		class if_t : public has_children {
		public:	
			enum type_t { if_regular, if_empty, if_rtl, if_cpp, if_else };
		private:
			class condition_t : public has_children {
				const type_t type_;
				const std::string cond_, variable_;
				const bool negate_;
			public:
				condition_t(type_t type, const std::string& cond, const std::string& variable, bool negate, base_ptr parent);
				virtual void dump(std::ostream& o, int tabs = 0);
				virtual void write(std::ostream& o);
				virtual base_ptr end(const std::string& what);
			};
			typedef std::shared_ptr<condition_t> condition_ptr;
			std::vector< condition_ptr > conditions_;
		public:
			if_t(base_ptr parent);
			
			base_ptr add_condition(type_t type, bool negate);
			base_ptr add_condition(const type_t& type, const std::string& variable, bool negate);
			base_ptr add_condition(const std::string& cond, bool negate);
			
			virtual void dump(std::ostream& o, int tabs = 0);
			virtual void write(std::ostream& o);
			virtual base_ptr end(const std::string& what);
		};

		class foreach_t : public base_t {
			class part_t : public has_children {
				bool has_end_;
			public:
				part_t(const std::string& sysname, bool has_end, base_ptr parent);
				virtual base_ptr end(const std::string& what);
			};
			const std::string name_, as_, rowid_, array_;
			const bool reverse_;
			base_ptr empty_, separator_, item_;
			base_ptr item_prefix_, item_suffix_;
		public:
			foreach_t(const std::string& name, const std::string& as, const std::string& rowid, const std::string& array, bool reverse, base_ptr parent);
			virtual void dump(std::ostream& o, int tabs = 0);
			virtual void write(std::ostream& o);
			virtual base_ptr end(const std::string& what);

			base_ptr prefix();
			base_ptr empty();
			base_ptr separator();
			base_ptr item();
			base_ptr suffix();
		};

		class cache_t : public has_children {
			const std::string name_, miss_;
			const int duration_;
			const bool recording_, triggers_;
			std::vector<std::string> trigger_list_;
		public:
			cache_t(const std::string& name, const std::string& miss, int duration, bool recording, bool triggers, base_ptr parent);
			base_ptr add_trigger(const std::string&);
			virtual void dump(std::ostream& o, int tabs = 0);
			virtual void write(std::ostream& o);
			virtual base_ptr end(const std::string& what);
		};




	}

	class parser {
		std::string input_;
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

		size_t index_;
		size_t failed_;
	public:
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
	private:
		bool try_flow_expression();
		bool try_global_expression();
		bool try_render_expression();
		bool try_variable_expression();
		
		// actions
		void add_html(const std::string&);
		void add_cpp(const std::string&);

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
 */

#endif
