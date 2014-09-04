#ifndef CPPCMS_TEMPLATES_COMPILER_AST_H
#define CPPCMS_TEMPLATES_COMPILER_AST_H

#include "parser_source.h"
#include "generator.h"
#include "errors.h"
#include "expr.h"
#include <memory>
#include <string>
#include <list>

// for demangle only
#include <cxxabi.h>

namespace {
	// FIXME: make better (aka: copy of boost demangle) wrapper, or use newer (1.56) boost
	std::string demangle(const char* name) {
		std::size_t size = 0;
		int status = 0;
		char *demangled = abi::__cxa_demangle( name, NULL, &size, &status );
		return demangled ? std::string(demangled) : ( std::string("[demangle failed:") + name + "]");
	}

	std::string translate_ast_object_name(const std::string& name) {
		const std::string prefix = "cppcms::templates::ast::";
		const std::string suffix = "_t";
		if(name == "cppcms::templates::ast::root_t")
			return "skin";
		else if(name == "cppcms::templates::ast::has_children_t")
			return "(any block node, like template, if, foreach, ...)";
		else if(name == "cppcms::templates::ast::cppcode_t")
			return "c++";
		else if(name == "cppcms::templates::ast::fmt_function_t")
			return "(format function, like gt, url, ...)";
		else if(name == "cppcms::templates::ast::if_t::condition_t")
			return "if";
		else if(name == "cppcms::templates::ast::foreach_t::part_t")
			return "foreach child (item, separator, empty, prefix, suffix)";
		else if(name.compare(0, prefix.length(), prefix) == 0 && name.compare(name.length() - suffix.length(), suffix.length(), suffix) == 0)
			return name.substr(prefix.length(), name.length()-prefix.length()-suffix.length());		
		else 
			return name;

	}
}
	

namespace cppcms { namespace templates { namespace ast {
	class base_t;
	class view_t;
	class root_t;
	class template_t;
	class has_children;

	typedef std::shared_ptr<base_t> base_ptr;
	typedef std::shared_ptr<view_t> view_ptr;
	typedef std::shared_ptr<root_t> root_ptr;
	typedef std::shared_ptr<template_t> template_ptr;
	typedef std::shared_ptr<has_children> has_children_ptr;
	
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
		base_ptr add_template(const expr::name& name, file_position_t line, const std::vector<expr::identifier> template_arguments, const expr::param_list& arguments);
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
		const std::vector<expr::identifier> template_arguments_;
		const expr::param_list arguments_;
	public:
		template_t(const expr::name& name, file_position_t line, const std::vector<expr::identifier>& template_arguments, const expr::param_list& arguments, base_ptr parent);
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
}}}

#endif
