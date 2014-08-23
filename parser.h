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
			base_ref parent_;
		protected:
			base_t(const std::string& sysname, base_ref parent);

			template<typename T>
			bool is_a() const { return dynamic_cast<T*>(this) != nullptr; }

			template<typename T>
			T& as() { return dynamic_cast<T&>(*this); }
		public:
			virtual void write(std::ostream&) = 0;
			base_ref parent();
		};

		class root_t : public base_t {
			typedef std::map< std::string, view_ptr > view_set_t;
			typedef std::map< std::string, view_set_t > skins_t;
			skins_t skins;
		public:
			root_t();
			virtual void write(std::ostream& o);
		};	
		
		class view_t : public base_t {
			typedef std::map<std::string, template_ptr> templates_t;
			templates_t templates;
			std::string name;
		public:
			view_t(const std::string& name, root_ref parent);
			virtual void write(std::ostream& o);
		};

		class template_t : public base_t {
			std::vector<base_ptr> children;
			std::string name_;
		public:
			template_t(const std::string& name, view_ref parent);
			void add(base_ptr what);
			virtual void write(std::ostream& o);
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
		size_t index_;
		size_t failed_;
	public:
		explicit parser(const std::string& input);
		parser& try_token(const std::string& token);
		parser& try_token_ws(const std::string& token);
		parser& try_name();
		parser& try_name_ws();
		parser& try_string();
		parser& try_string_ws();
		parser& try_number();
		parser& try_number_ws();
		parser& try_variable();
		parser& try_variable_ws();
		parser& try_identifier();
		parser& try_identifier_ws();
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
		ast::root_ptr tree_;
	public:
		template_parser(const std::string& input);
		bool try_flow_expression();
		bool try_global_expression();
		bool try_render_expression();
		bool try_variable_expression();

		void parse();

		// actions
		void add_html(const std::string&);
		void add_cpp(const std::string&);
	};

}}

/*
 * arguments to do: 
 * 	-s (skin name), which replaces __default__ skin name in skin map (compat)
 * 	-m (magic includes, uses data::foobar automagically includes data/foobar.h) (new)
 */

#endif
