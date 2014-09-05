#ifndef CPPCMS_TEMPLATE_COMPILER_EXPR_H
#define CPPCMS_TEMPLATE_COMPILER_EXPR_H

#include "generator.h"

#include <memory>
#include <string>

namespace cppcms { namespace templates { namespace expr {
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
			const ptr subscript;
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
		ptr parse_subscript(const std::string&, size_t&);	
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
}}}

#endif
