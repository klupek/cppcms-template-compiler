#include "ast.h"
#include <algorithm>
#include <sstream>
#include <boost/lexical_cast.hpp>

namespace cppcms { namespace templates { namespace ast {
	struct ln { 
		const file_position_t line_;
		ln(file_position_t line) : line_(line) {}
	};
	
	static std::ostream& operator<<(std::ostream&o, const ln& obj) {
		return o << "#line " << obj.line_.line << " \"" << obj.line_.filename << "\"\n";
	}
	

	base_t::base_t(const std::string& sysname, file_position_t line, bool block, base_ptr parent)
		: sysname_(sysname)
		, parent_(parent) 
		, block_(block) 
		, line_(line) {}
	file_position_t base_t::line() const {
		return line_; 
	}	
	base_ptr base_t::parent() { return parent_; }
	
	const std::string& base_t::sysname() const {
		return sysname_;
	}

	bool base_t::block() const { return block_; }

	root_t::root_t() 
		: base_t("root", file_position_t{"__root__", 0}, true, nullptr)		
		, current_skin(skins.end()) {}

	base_ptr root_t::add_skin(const expr::name& name, file_position_t line) {		
		current_skin = std::find_if(skins.begin(), skins.end(), [=](const skins_t::value_type& skin) {
			return skin.first.repr() == name->repr();
		});
		if(current_skin == skins.end()) {
			skins.emplace_back( *name, skin_t { line, line, view_set_t() } );
			current_skin = --skins.end();
		}
		return shared_from_this();
	}

	base_ptr root_t::set_mode(const std::string& mode, file_position_t line) {
		mode_ = mode;
		mode_line_ = line;
		return shared_from_this();
	}

	base_ptr root_t::add_cpp(const expr::cpp& code, file_position_t line) {
		codes.emplace_back(code_t { line, code });
		return shared_from_this();
	}
	
	base_ptr root_t::add_view(const expr::name& name, file_position_t line, const expr::identifier& data, const expr::name& parent) {
		if(current_skin == skins.end())
			throw std::runtime_error("view must be inside skin");
	
		auto i = std::find_if(current_skin->second.views.begin(), current_skin->second.views.end(), [=](const view_set_t::value_type& ve) {
			return ve.first.repr() == name->repr();
		});
		if(i == current_skin->second.views.end()) {
			current_skin->second.views.emplace_back(
				*name, std::make_shared<view_t>(name, line, data, parent, shared_from_this())
			);
			i = --current_skin->second.views.end();
		}
		return i->second;
	}

	std::string root_t::mode() const { 
		return mode_;
	}

	base_ptr root_t::end(const std::string& what, file_position_t line) {
		const std::string current = (current_skin == skins.end() ? "__root" : "skin");
		if(what.empty() || what == current) {
			current_skin->second.endline = line; 
			current_skin = skins.end();
		} else if(current == "skin") {
			throw std::runtime_error("expected 'end skin', not 'end " + what + "'");
		} else {
			throw std::runtime_error("unexpected 'end '" + what + "'");
		}
		return shared_from_this();
	}

	void root_t::write(generator::context& context, std::ostream& output) {			
		// checks
		// check if there is at least one skin
		if(skins.empty()) {
			throw std::runtime_error("No skins defined");
		}

		// check if there is no conflict between [ -s NAME ] argument and defined skins
		auto i = std::find_if(skins.begin(), skins.end(), [](const skins_t::value_type& skin) {
			return skin.first.repr() == "__default__";
		});

		if(i != skins.end()) {
			if(context.skin.empty()) {
				throw error_at_line("Requested default skin name, but none was provided in arguments", i->second.line);
			} else {
				skins.emplace_back(expr::name_t(context.skin), i->second);
				skins.erase(i);
			}
		}

		std::ostringstream buffer;
		
		for(const code_t& code : codes) {
			buffer << ln(code.line) << code.code->code(context) << std::endl;
		}
		
		for(const skins_t::value_type& skin : skins) {
			if(!context.skin.empty() && context.skin != skin.first.repr())
				throw error_at_line("Mismatched skin names, in argument and template source", skin.second.line);
			buffer << ln(skin.second.line);
			buffer << "namespace " << skin.first.code(context) << " {\n";
			context.current_skin = skin.first.repr();
			for(const view_set_t::value_type& view : skin.second.views) {
				view.second->write(context, buffer);
			}
			buffer << ln(skin.second.endline);
			buffer << "} // end of namespace " << skin.first.code(context) << "\n";
		}
		file_position_t pll = skins.rbegin()->second.endline; // past last line
		pll.line++;

		for(const auto& skinpair : context.skins) {
			buffer << "\n" << ln(pll) << "namespace {\n" << ln(pll) << "cppcms::views::generator my_generator;\n" << ln(pll) << "struct loader {";
			buffer << ln(pll) << "loader() {\n" << ln(pll);			
			buffer << "my_generator.name(\"" << skinpair.first << "\");\n";
			for(const auto& view : skinpair.second.views) {
				buffer << ln(pll) << "my_generator.add_view< " << skinpair.first << "::" << view.name << ", " << view.data << " >(\"" << view.name << "\", true);\n";
			}
			buffer << ln(pll) << "cppcms::views::pool::instance().add(my_generator);\n";
			buffer << ln(pll) << "}\n";
			buffer << ln(pll) << "~loader() { cppcms::views::pool::instance().remove(my_generator); }\n";
			buffer << ln(pll) << "} a_loader;\n";
			buffer << ln(pll) << "} // anon \n";
		}
		
		for(const generator::context::include_t& include : context.includes) {
			output << "#include <" << include << ">\n";
		}
		output << buffer.str();
	}
	
	void view_t::write(generator::context& context, std::ostream& o) {
		context.skins[context.current_skin].views.emplace_back( generator::context::view_t { name_->code(context), data_->code(context) });
		o << ln(line());
		o << "struct " << name_->code(context) << ":public ";
		if(master_)
			o << master_->code(context);
		else
			o << "cppcms::base_view\n";
		o << ln(line()) << " {\n";
		
		o << ln(line());
		o << data_->code(context) << " & content;\n";
		
		o << ln(line());
		o << name_->code(context) << "(std::ostream & _s, " << data_->code(context) << " & _content):";
		if(master_)
			o << master_->code(context) << "(_s, _content)";
		else 
			o << "cppcms::base_view(_s)";
		o << ",content(_content)\n" << ln(line()) << "{\n" << ln(line()) << "}\n";

		for(const templates_t::value_type& tpl : templates) {
			tpl.second->write(context, o);
		}

		o << ln(endline_) << "}; // end of class " << name_->code(context) << "\n";
	}
		
	void root_t::dump(std::ostream& o, int tabs)  const {
		std::string p(tabs, '\t');
		o << p << "root with " << codes.size() << " codes, mode = " << (mode_.empty() ? "(default)" : mode_) << " [\n";
		for(const skins_t::value_type& skin : skins) {
			o << p << "\tskin " << skin.first << " with " << skin.second.views.size() << " views [\n";
			for(const view_set_t::value_type& view : skin.second.views) {
				view.second->dump(o, tabs+2);
			}
			o << p << "\t]\n";
		}
		o << p << "]; codes = [\n";
		for(const code_t& code : codes)
			o << p << "\t" << *code.code << std::endl;
		o << p << "];\n";
	}

	text_t::text_t(const expr::ptr& value, file_position_t line, base_ptr parent)  
		: base_t("text", line, false, parent)
		, value_(value) {}

	void text_t::dump(std::ostream& o, int tabs)  const {
		const std::string p(tabs, '\t');
		o << p << "text: " << *value_ << std::endl;			
	}

	void text_t::write(generator::context& context, std::ostream& o) {
		o << ln(line()) << "out() << " << value_->code(context) << ";\n";
	}
	base_ptr text_t::end(const std::string&, file_position_t) {
		throw std::logic_error("unreachable code -- this is not block node");			
	}


	base_ptr view_t::end(const std::string& what,file_position_t line) {
		if(what.empty() || what == "view") {
			endline_ = line;
			return parent();
		} else {
			throw std::runtime_error("expected 'end view', not 'end " + what + "'");
		}
	}

	void template_t::write(generator::context& context, std::ostream& o) {
		if(!template_arguments_.empty()) {
			o << ln(line()) << "template<";
			for(auto i = template_arguments_.begin(); i != template_arguments_.end(); ++i) {
				const expr::identifier& x = *i;
				if(i != template_arguments_.begin()) 
					o << ", ";
				o << "typename " << x->code(context);
			}
			o << ">\n" << ln(line()) << "void " << name_->code(context) << arguments_->code(context) << "{\n";
		} else {
			o << ln(line()) << "virtual void " << name_->code(context) << arguments_->code(context) << "{\n";
		}
		for(const auto& param : arguments_->params()) {
			context.add_scope_variable(param.name->code(context));
		}
		for(const base_ptr child : children) {
			child->write(context, o);
		}
		
		for(const auto& param : arguments_->params()) {
			context.remove_scope_variable(param.name->code(context));
		}
		o << ln(endline_) << "} // end of template " << name_->code(context) << "\n";
	}

	base_ptr view_t::add_template(const expr::name& name, file_position_t line, const std::vector<expr::identifier> template_arguments, const expr::param_list& arguments) {
		templates.emplace_back(
			*name, std::make_shared<template_t>(name, line, template_arguments, arguments, shared_from_this())
		);
		return templates.back().second; 
	}
	view_t::view_t(const expr::name& name, file_position_t line, const expr::identifier& data, const expr::name& master, base_ptr parent)
		: base_t("view", line, true, parent)
		, name_(name)
		, master_(master) 
		, data_(data)
		, endline_(line)	{}
	
	void view_t::dump(std::ostream& o, int tabs)  const {
		const std::string p(tabs, '\t');
		o << p << "view " << *name_ << " uses " << *data_ << " extends ";
		if(master_)
			o << *master_;
		else
			o << "(default)";

		o << " with " << templates.size() << " templates {\n";
		for(const templates_t::value_type& templ : templates) {
			templ.second->dump(o, tabs+1);
		}
		o << p << "}\n";
	}
	
	has_children::has_children(const std::string& sysname, file_position_t line, bool block, base_ptr parent) 
		: base_t(sysname, line, block, parent)
		, endline_(line)	{}
	
	file_position_t has_children::endline() const { return endline_; }
	void has_children::dump(std::ostream& o, int tabs)  const {
		for(const base_ptr& child : children) 
			child->dump(o, tabs);
	}
	
	void has_children::write(generator::context& context, std::ostream& o) {
		for(const base_ptr& child : children) {
			child->write(context, o);
		}
	}

	template_t::template_t(const expr::name& name, file_position_t line, const std::vector<expr::identifier>& template_arguments, const expr::param_list& arguments, base_ptr parent) 
		: has_children("template", line, true, parent)
		, name_(name) 
		, template_arguments_(template_arguments)
		, arguments_(arguments) {}

	base_ptr template_t::end(const std::string& what,file_position_t line) {
		if(what.empty() || what == "template") {
			endline_ = line;
			return parent();
		} else {
			throw std::runtime_error("expected 'end template', not 'end " + what + "'");
		}
	}

	void template_t::dump(std::ostream& o, int tabs)  const {
		const std::string p(tabs, '\t');
		o << p << "template " << *name_ << " with arguments " << *arguments_ << " and " << children.size() << " children [\n";
		for(const base_ptr& child : children)
			child->dump(o, tabs+1);
		o << p << "]\n";
	}
	
	cppcode_t::cppcode_t(const expr::cpp& code, file_position_t line, base_ptr parent)
		: base_t("c++", line, false, parent)
		, code_(code) {}

	void cppcode_t::dump(std::ostream& o, int tabs)  const {
		const std::string p(tabs, '\t');
		o << p << "c++: " << code_ << std::endl;
	}

	void cppcode_t::write(generator::context& context, std::ostream& o) {
		o << ln(line()) << code_->code(context) << std::endl;
	}

	base_ptr cppcode_t::end(const std::string&, file_position_t) {
		throw std::logic_error("end in non-block component");
	}
	
	variable_t::variable_t(const expr::variable& name, file_position_t line, const std::vector<expr::filter>& filters, base_ptr parent)
		: base_t("variable", line, false, parent)
		, name_(name)
		, filters_(filters) {}

	void variable_t::dump(std::ostream& o, int tabs)  const {
		const std::string p(tabs, '\t');
		o << p << "variable: " << *name_;
		if(filters_.empty())
			o << " without filters\n";
		else {
			o << " with filters: "; 
			for(const expr::filter& filter : filters_) 
				o << " | " << *filter;
			o << std::endl;
		}
	}
	
	base_ptr variable_t::end(const std::string&, file_position_t) {
		throw std::logic_error("end in non-block component");
	}

	std::string variable_t::code(generator::context& context, const std::string& escaper) const {
		const std::string escape = ( escaper.empty() ? "" : escaper + "(" );
		const std::string close = ( escaper.empty() ? "" : ")" );
		if(filters_.empty()) {				
			return escape + name_->code(context) + close;
		} else {
			std::string current = name_->code(context);
			for(auto i = filters_.rbegin(); i != filters_.rend(); ++i) {
				expr::filter filter = *i;
				current = filter->argument(current).code(context);
			}
			return current;
		}
	}

	void variable_t::write(generator::context& context, std::ostream& o) {
		o << ln(line());
		o << "out() << " << code(context) << ";\n";
	}
		
	fmt_function_t::fmt_function_t(	const std::string& name,
					file_position_t line,	
					const expr::string& fmt, 
					const using_options_t& uos, 
					base_ptr parent) 
		: base_t(name, line, false, parent)
		, name_(name)
		, fmt_(fmt)
		, using_options_(uos) {}
	
	void fmt_function_t::dump(std::ostream& o, int tabs)  const {
		const std::string p(tabs, '\t');
		o << p << "fmt function " << name_ << ": " << fmt_->repr() << std::endl;
		if(using_options_.empty()) {
			o << p << "\twithout using\n";
		} else {
			o << p << "\twith using options:\n"; 
			for(const using_option_t& uo : using_options_) {
				uo.dump(o, tabs+2);
			}
		}
	}
	
	base_ptr fmt_function_t::end(const std::string&, file_position_t) {
		throw std::logic_error("end in non-block component");
	}

	void fmt_function_t::write(generator::context& context, std::ostream& o) {						
		o << ln(line());
		std::string function_name;

		if(name_ == "gt") {
			function_name = "cppcms::locale::translate";
		} else if(name_ == "url") {
			o << "content.app().mapper().map(out(), " << fmt_->code(context);
			for(const using_option_t& uo : using_options_) {
				o << ", " << uo.code(context, "cppcms::filters::urlencode");
			}
			o << ");\n";
			return;
		} else if(name_ == "format") {
			context.add_include("boost/format.hpp");
			o << "out() << cppcms::filters::escape("
				<< "(boost::format(" << fmt_->code(context) << ")";
			for(const using_option_t& uo : using_options_) {
				o << "% (" << uo.code(context, "") << ")";					
			}
			o << ").str());";
			return;
		} else if(name_ == "rformat") {
			context.add_include("boost/format.hpp");
			o << "out() << (boost::format(" << fmt_->code(context) << ")";
			for(const using_option_t& uo : using_options_) {
				o << "% (" << uo.code(context, "") << ")";					
			}
			o << ").str();";
			return;
		}
		if(using_options_.empty()) {
			o << "out() << " << function_name << "(" << fmt_->code(context) << ");\n";
		} else {
			o << "out() << cppcms::locale::format(" << function_name << "(" << fmt_->code(context) << ")) ";
			for(const using_option_t& uo : using_options_) {
				o << " % (" << uo.code(context) << ")";
			}
			o << ";\n";
		}
	}
	
	ngt_t::ngt_t(	file_position_t line,
			const expr::string& singular, 
			const expr::string& plural,
			const expr::variable& variable,
			const using_options_t& uos, 
			base_ptr parent)
		: base_t("ngt", line, false, parent)
		, singular_(singular)
		, plural_(plural)
		, variable_(variable) 
		, using_options_(uos) {}
	
	void ngt_t::dump(std::ostream& o, int tabs)  const {
		const std::string p(tabs, '\t');
		o << p << "fmt function ngt: " << *singular_ << "/" << *plural_ << " with variable " << *variable_ <<  std::endl;
		if(using_options_.empty()) {
			o << p << "\twithout using\n";
		} else {
			o << p << "\twith using options:\n"; 
			for(const using_option_t& uo : using_options_) {
				uo.dump(o,tabs+2);
			}
		}
	}
	
	base_ptr ngt_t::end(const std::string&, file_position_t) {
		throw std::logic_error("end in non-block component");
	}

	void ngt_t::write(generator::context& context, std::ostream& o) {
		o << ln(line());
		const std::string function_name = "cppcms::locale::translate";
		
		if(using_options_.empty()) {
			o << "out() << " << function_name << "(" 
				<< singular_->code(context) << ", " 
				<< plural_->code(context) << ", " 
				<< variable_->code(context) << ");\n";
		} else {
			o << "out() << cppcms::locale::format(" << function_name << "(" 
				<< singular_->code(context) << ", " 
				<< plural_->code(context) << ", "
				<< variable_->code(context) << ")) ";
			for(const using_option_t& uo : using_options_) {
				o << " % (" << uo.code(context) << ")";
			}
			o << ";\n";
		}
	}
		
	include_t::include_t(	const expr::call_list& name, file_position_t line, const expr::identifier& from, 
				const expr::identifier& _using, const expr::variable& with, 
				base_ptr parent) 
		: base_t("include", line, false, parent) 
		, name_(name)
		, from_(from)
		, using_(_using) 
		, with_(with) {}
	
	void include_t::dump(std::ostream& o, int tabs)  const {
		const std::string p(tabs, '\t');
		o << p << "include " << *name_;

		if(!from_) {
			if(using_) {
				o << " using " << *using_;
				if(with_)
					o << " with " << *with_;
				else
					o << " with (this content)";
			} else {
				o << " from (self)";
			}
		} else if(from_) {
			o << " from " << *from_;
		} else {
			o << " from (self)";
		}
		o << std::endl;
	}
	
	base_ptr include_t::end(const std::string&, file_position_t) {
		throw std::logic_error("end in non-block component");
	}

	void include_t::write(generator::context& context, std::ostream& o) {
		o << ln(line());
		if(from_) {
			if(!context.check_scope_variable(from_->code(context))) {
				throw error_at_line("No local view variable " + from_->code(context) + " found in context.", line());
			}
			o << name_->code(context) << ";";
		} else if(using_) {
			o << "{\n";
			if(with_) {
				o << ln(line()) << "cppcms::base_content::app_guard _g(" << with_->code(context) << ", content);\n";
			}
			o << ln(line()) << using_->code(context) << " _using(out(), ";
			if(with_) {
				o << with_->code(context);
			} else {
				o << "content";
			} 
			o << ");\n";
			o << ln(line());
			o << name_->code(context) << ";\n";
			o << ln(line()) << "}";
		} else {
			o << name_->code(context) << ";";
		}
		o << "\n";
	}

	form_t::form_t(const expr::name& style, file_position_t line, const expr::variable& name, base_ptr parent)
		: has_children("form", line, ( style && ( style->repr() == "block" || style->repr() == "begin")), parent)
		, style_(style)
		, name_(name) {}
	
	void form_t::dump(std::ostream& o, int tabs)  const {
		const std::string p(tabs, '\t');
		o << p << "form style = " << *style_ << " using variable " << *name_ << std::endl;
	}
	
	base_ptr form_t::end(const std::string& x, file_position_t) {
		if(style_->repr() == "block" || style_->repr() == "begin") {
			if(x.empty() || x == "form") {
				return parent();
			} else {
				throw std::runtime_error("Unexpected 'end " + x + "', expected 'end form'");
			}
		} else {
			throw std::logic_error("end in non-block component");
		}
	}
	
	void form_t::write(generator::context& context, std::ostream& o) {
		const std::string mode = context.output_mode;
		if(style_->repr() == "as_table" || style_->repr() == "as_p" || 
				style_->repr() == "as_ul" || style_->repr() == "as_dl" ||
				style_->repr() == "as_space") {
			o << ln(line()) << "{ ";
			o << "cppcms::form_context _form_context(out(), cppcms::form_flags::as_" << mode << ", cppcms::form_flags::" << style_->code(context) << "); ";
			o << "(" << name_->code(context) << ").render(_form_context); ";
			o << "}\n";
		} else if(style_->repr() == "input") {
			o << ln(line()) << " { ";
			o << "cppcms::form_context _form_context(out(),cppcms::form_flags::as_" << mode << ");\n";
			o << ln(line()) << "_form_context.widget_part(cppcms::form_context::first_part);\n";
			o << ln(line()) << "(" << name_->code(context) << ").render_input(_form_context); ";
			o << ln(line()) << "out() << (" << name_->code(context) << ").attributes_string();\n";
			o << ln(line()) << "_form_context.widget_part(cppcms::form_context::second_part);\n";
			o << ln(line()) << "(" << name_->code(context) << ").render_input(_form_context);\n";
			o << ln(line()) << "}\n";
		} else if(style_->repr() == "begin" || style_->repr() == "block") {
			o << ln(line()) << " { ";
			o << "cppcms::form_context _form_context(out(),cppcms::form_flags::as_" << mode << ");\n";
			o << ln(line()) << "_form_context.widget_part(cppcms::form_context::first_part);\n";
			o << ln(line()) << "(" << name_->code(context) << ").render_input(_form_context); ";
			o << ln(line()) << "}\n";
			for(const base_ptr& child : children) {
				child->write(context, o);
			}
			o << ln(endline_) << " { ";
			o << "cppcms::form_context _form_context(out(),cppcms::form_flags::as_" << mode << ");\n";
			o << ln(endline_) << "_form_context.widget_part(cppcms::form_context::second_part);\n";
			o << ln(endline_) << "(" << name_->code(context) << ").render_input(_form_context);\n";
			o << ln(endline_) << "}\n";
		}	
	}
	
	csrf_t::csrf_t(file_position_t line, const expr::name& style, base_ptr parent)
		: base_t("csrf", line, false, parent)
		, style_(style) {}
	
	void csrf_t::dump(std::ostream& o, int tabs)  const {
		const std::string p(tabs, '\t');
		if(style_)
			o << p << "csrf style = " << *style_ << "\n";
		else
			o << p << "csrf style = (default)\n";
	}
	
	base_ptr csrf_t::end(const std::string&, file_position_t) {
		throw std::logic_error("end in non-block component");
	}
	
	void csrf_t::write(generator::context&, std::ostream& o) {
		if(!style_) {
			o << ln(line()) << "out() << \"<input type=\\\"hidden\\\" name=\\\"_csrf\\\" value=\\\"\" << content.app().session().get_csrf_token() << \"\\\" >\\n\"\n;";
		} else if(style_->repr() == "token") {
			o << ln(line()) << "out() << content.app().session().get_csrf_token();\n";
		} else if(style_->repr() == "script")  {
			std::string jscode = R"javascript(                        out() << "\n"
			"            <script type='text/javascript'>\n"
			"            <!--\n"
			"                {\n"
			"                    var cppcms_cs = document.cookie.indexOf(\""<< content.app().session().get_csrf_token_cookie_name() <<"=\");\n"
			"                    if(cppcms_cs != -1) {\n"
			"                        cppcms_cs += '"<< content.app().session().get_csrf_token_cookie_name() <<"='.length;\n"
			"                        var cppcms_ce = document.cookie.indexOf(\";\",cppcms_cs);\n"
			"                        if(cppcms_ce == -1) {\n"
			"                            cppcms_ce = document.cookie.length;\n"
			"                        }\n"
			"                        var cppcms_token = document.cookie.substring(cppcms_cs,cppcms_ce);\n"
			"                        document.write('<input type=\"hidden\" name=\"_csrf\" value=\"' + cppcms_token + '\" >');\n"
			"                    }\n"
			"                }\n"
			"            -->\n"
			"            </script>\n"
			"            ";)javascript";
			o << ln(line()) << jscode << "\n";
		} else if(style_->repr() == "cookie") {
			o << ln(line()) << "out() << content.app().session().get_csrf_token_cookie_name();\n";
		} else {
			throw std::logic_error("Invalid csrf style: " + style_->repr());
		}

	}
	
	render_t::render_t(file_position_t line, const expr::ptr& skin, const expr::ptr& view, const expr::variable& with, base_ptr parent)
		: base_t("render", line, false, parent)
		, skin_(skin)
		, view_(view)
		, with_(with) {}
	
	void render_t::dump(std::ostream& o, int tabs)  const {
		const std::string p(tabs, '\t');
		o << p << "render skin = ";
		if(skin_)
			o << *skin_;
		else 
			o << "(current)";

		o << ", view = " << *view_ << " with " ;
		if(with_)
			o << *with_;
		else 
			o << "(current)";
		o << " content\n";
	}
	
	base_ptr render_t::end(const std::string&, file_position_t) {
		throw std::logic_error("end in non-block component");
	}
	
	void render_t::write(generator::context& context, std::ostream& o) {
		o << ln(line()) << "{\n";
		if(with_) {
			o << ln(line());
			o << "cppcms::base_content::app_guard _g(" << with_->code(context) << ", content);\n";
		}
		o << ln(line()) << "cppcms::views::pool::instance().render(";
		if(skin_) {
			o << skin_->code(context);
		} else {
			o << '"' << context.current_skin << '"';
		}
		o << ", " << view_->code(context) << ", out(), ";
		if(with_)
			o << with_->code(context);
		else
			o << "content";
		o << ");\n";
		o << ln(line()) << "}\n";
	}
		
	using_t::using_t(file_position_t line, const expr::identifier& id, const expr::variable& with, const expr::identifier& as, base_ptr parent)
		: has_children("using", line, true, parent)
		, id_(id)
		, with_(with)
		, as_(as) {}
	
	void using_t::dump(std::ostream& o, int tabs)  const {
		const std::string p(tabs, '\t');
		o << p << "using view type " << *id_ << " as " << *as_ << " with ";
		if(with_)
			o << *with_;
		else
			o << "(current)";
		o << " content [\n";
		for(const base_ptr& child : children)
			child->dump(o, tabs+1);
		o << p << "]\n";
	}
	
	base_ptr using_t::end(const std::string& what,file_position_t line) {
		if(what.empty() || what == "using") {
			endline_ = line;
			return parent();
		} else {
			throw std::runtime_error("expected 'end using', not 'end '" + what + "'");
		}
	}
	
	void using_t::write(generator::context& context, std::ostream& o) {
		o << ln(line()) << "{\n";
		if(with_) {
			o << ln(line()) << "cppcms::base_content::app_guard _g(" << with_->code(context) << ", content);\n";
		}
		o << ln(line()) << id_->code(context) << " " << as_->code(context) << "(out(), ";
		if(with_) {
			o << with_->code(context);
		} else {
			o << "content";
		}
		o << ");\n";
		context.add_scope_variable(as_->code(context));
		for(const base_ptr& child : children) {
			child->write(context, o);
		}
		context.remove_scope_variable(as_->code(context));
		o << ln(endline_) << "}\n";
	}
			
	
	if_t::condition_t::condition_t(file_position_t line, type_t type, const expr::cpp& cond, const expr::variable& variable, bool negate, base_ptr parent)
		: has_children("condition", line, true, parent)
		, type_(type)
		, cond_(cond)
		, variable_(variable) 
		, negate_(negate) {}
		
	if_t::if_t(file_position_t line, base_ptr parent)
		: has_children("if", line, true, parent) {}

	void if_t::condition_t::add_next(const next_op_t& no, const type_t& type, const expr::variable& variable, bool negate) {
		next.push_back({
				std::make_shared<condition_t>(line(), type, expr::cpp(), variable, negate, parent()),
				no });
	}

	base_ptr if_t::add_condition(file_position_t line, type_t type, bool negate) {
		if(!conditions_.empty())
			conditions_.back()->end(std::string(), line);
		conditions_.emplace_back(
			std::make_shared<condition_t>(line, type, expr::cpp(), expr::variable(), negate, shared_from_this())
		);
		return conditions_.back();
	}
		
	base_ptr if_t::add_condition(file_position_t line, const type_t& type, const expr::variable& variable, bool negate) {
		if(!conditions_.empty())
			conditions_.back()->end(std::string(), line);
		conditions_.emplace_back(
			std::make_shared<condition_t>(line, type, expr::cpp(), variable, negate, shared_from_this())
		);
		return conditions_.back();
	}
		
	base_ptr if_t::add_condition(file_position_t line, const expr::cpp& cond, bool negate) {
		if(!conditions_.empty())
			conditions_.back()->end(std::string(), line);
		conditions_.emplace_back(
			std::make_shared<condition_t>(line, type_t::if_cpp, cond, expr::variable(), negate, shared_from_this())
		);
		return conditions_.back();
	}
	
	void if_t::dump(std::ostream& o, int tabs)  const {
		const std::string p(tabs, '\t');
		o << p << "if with " << conditions_.size() << " branches [\n";
		for(const condition_ptr& c : conditions_)
			c->dump(o, tabs+1);
		o << p << "]\n";
	}
	
	base_ptr if_t::end(const std::string&, file_position_t) {
		throw std::logic_error("unreachable code (or rather: bug)");
	}
	
	void if_t::write(generator::context& context, std::ostream& o) {
		auto condition = conditions_.begin();
		(*condition)->write(context, o);

		for(++condition; condition != conditions_.end(); ++condition) {
			if((*condition)->type() == type_t::if_else ) {
				o << " else ";
			} else {
				o << "\n" << ln((*condition)->line()) << "else\n";
			}
			(*condition)->write(context, o);
		}
		if(conditions_.back()->type() == type_t::if_else) {
			o << "\n";
		} else {
			o << " // endif\n";
		}
	}

	if_t::type_t if_t::condition_t::type() const { return type_; }

	void if_t::condition_t::dump(std::ostream& o, int tabs)  const {
		const std::string p(tabs, '\t');
		auto printer = [&o,&p](const condition_t& self) {
			const std::string neg = (self.negate_ ? "not " : "");
			switch(self.type_) {
				case type_t::if_regular:
					o << p << neg << "true: " << *self.variable_;
					break;
				case type_t::if_empty:
					o << p << neg << "empty: " << *self.variable_;
					break;
				case type_t::if_rtl:
					o << p << neg << "rtl";
					break;
				case type_t::if_cpp:
					o << p  << neg << "cpp: " << *self.cond_;
					break;
				case type_t::if_else:
					o << p << "else: ";
					break;
			}
		};
		o << "if ";
		printer(*this);
		for(const auto& pair : next) {
			if(pair.second == next_op_t::op_or)
				o << " or ";
			else 
				o << " and ";
			printer(*pair.first);
		}

		o << " [\n";
		for(const base_ptr& bp : children) {
			bp->dump(o, tabs+1);
		}
		o << p << "]\n";
	}
	
	base_ptr if_t::condition_t::end(const std::string& what,file_position_t line) {
		if(what.empty() || what == "if") {
			endline_ = line;
			return parent()->parent(); // this <- if_t <- if_parent, aka end if statement
		} else {
			throw std::runtime_error("expected 'end if', not 'end " + what + "', if started at line " + this->line().filename + ":" + boost::lexical_cast<std::string>(this->line().line));
		}
	}
	
	void if_t::condition_t::write(generator::context& context, std::ostream& o) {			
		if(type_ != type_t::if_else) {
			o << ln(line());
			o << "if(";
		}

		auto printer = [&o,&context](const condition_t& self) {
			if(self.negate_)
				o << "!(";

			switch(self.type_) {
			case type_t::if_regular:
				o << "" << self.variable_->code(context) << "";
				break;

			case type_t::if_empty:
				o << "" << self.variable_->code(context) << ".empty()";
				break;

			case type_t::if_rtl:
				o << "(cppcms::locale::translate(\"LTR\").str(out().getloc()) == \"RTL\")";
				break;
			
			case type_t::if_cpp:
				o << "" << self.cond_->code(context) << "";
				break;

			case type_t::if_else:
				break;				
		
			}

			if(self.negate_)
				o << ")";
		};

		printer(*this);
		for(const auto& pair : next) {
			if(pair.second == next_op_t::op_or) {
				o << " || ";
			} else {
				o << " && ";
			}
			printer(*pair.first);
		}
		
		
		if(type_ != type_t::if_else) {
			o << ") {\n";
		} else {
			o << " {\n";
		}
		for(const base_ptr& bp : children) {
			bp->write(context, o);
		}
		o << ln(endline_) << "} ";
	}
		
	foreach_t::foreach_t(	file_position_t line, 
				const expr::name& name, const expr::identifier& as, 
				const expr::name& rowid, const int from,
				const expr::variable& array, bool reverse, base_ptr parent) 
		: base_t("foreach", line, true, parent) 
		, name_(name)
		, as_(as)
		, rowid_(rowid)
		, from_(from)
		, array_(array) 
		, reverse_(reverse) {}
		
	void foreach_t::dump(std::ostream& o, int tabs)  const {
		const std::string p(tabs, '\t');
		o << p << "foreach " << *name_;
		if(as_)
			o << " (as " << *as_ << ")";
		if(rowid_)
			o << " (and rowid named " << *rowid_ << ")";			
		o << " starting from row " << from_;
		o << " in " << (reverse_ ? "reversed array " : "array ") << *array_ << "{\n";
		if(empty_) {
			o << p << "\tempty = [\n";
			empty_->dump(o, tabs+2);
			o << p << "\t]\n";
		} else { 
			o << p << "\tempty not set\n";
		}

		if(separator_) {
			o << p << "\tseparator = [\n";
			separator_->dump(o, tabs+2);
			o << p << "\t]\n";
		} else {
			o << p << "\tseparator not set\n";
		}

		if(item_) {
			o << p << "\titem = [\n";
			item_->dump(o,tabs+2);
			o << p << "\t]\n";
		} else {
			o << p << "\titem not set\n";
		}
		
		if(item_prefix_) {
			o << p << "\titem prefix = [\n";
			item_prefix_->dump(o,tabs+2);
			o << p << "\t]\n";
		} else {
			o << p << "\titem prefix not set\n";
		}
		
		if(item_suffix_) {
			o << p << "\titem suffix = [\n";
			item_suffix_->dump(o,tabs+2);
			o << p << "\t]\n";
		} else {
			o << p << "\titem suffix not set\n";
		}
		o << p << "}\n";
	}
	
	base_ptr foreach_t::end(const std::string&, file_position_t) {
		throw std::logic_error("unreachable code (or rather: bug)");
	}
	
	void foreach_t::write(generator::context& context, std::ostream& o) {
		const std::string array = "(" + array_->code(context) + ")";
		const std::string item = name_->code(context);
		const std::string rowid = (rowid_ ? rowid_->code(context) : "__rowid");
		const std::string type = (as_ ? as_->code(context) : ("CPPCMS_TYPEOF(" + array + ".begin())"));
		const std::string vtype = (as_ ? ("std::iterator_traits <" + type + " >::value_type") : ("CPPCMS_TYPEOF(*" + item + "_ptr)"));

		o << ln(line());
		o << "if(" << array << ".begin() != " << array << ".end()) {\n";
		if(rowid_) {
			o << ln(line()) << "int " << rowid << " = 1;\n";
		}
		if(item_prefix_)
			item_prefix_->write(context, o);

		o << ln(item_->line());
		o << "for (" <<  type << " "<< item << "_ptr = " << array << ".begin(), " << item << "_ptr_end = " << array << ".end(); " 
			<< item << "_ptr != " << item << "_ptr_end; ++" << item << "_ptr";
		if(rowid_)
			o << ", ++" << rowid << ") {\n";
		else
			o << ") {\n";

		o << ln(item_->line());
		o << vtype <<" & " << item << " = *" << item << "_ptr;\n";
		
		if(rowid_) 
			context.add_scope_variable(rowid);
		context.add_scope_variable(item);
		if(separator_) {
			o << ln(separator_->line());
			o << "if(" << item << "_ptr != " << array << ".begin()) {\n";
			separator_->write(context, o);
			o << ln(separator_->endline()) << "} // end of separator\n";
		}
		item_->write(context, o);			

		if(rowid_) 
			context.remove_scope_variable(rowid);
		context.remove_scope_variable(item);
		o << ln(item_->endline()) << "} // end of item\n";

		if(item_suffix_)
			item_suffix_->write(context, o);

		if(empty_) {
			o << ln(empty_->line());
			o << "} else {\n";
			empty_->write(context, o);
			o << ln(empty_->endline()) << "} // end of empty\n";

		} else {
			if(item_suffix_)
				o << ln(item_suffix_->endline());
			else 
				o << ln(item_->endline());
			o << "}\n";
		}

	}

	foreach_t::part_t::part_t(file_position_t line, const std::string& sysname, bool has_end, base_ptr parent) 
		: has_children(sysname, line, true, parent)
		, has_end_(has_end) {}

	base_ptr foreach_t::part_t::end(const std::string& what, file_position_t line) {
		if(has_end_) { // aka: it is 'item'
			if(what.empty() || what == sysname()) {
				endline_ = line;
				return parent()->as<foreach_t>().suffix(line);
			} else {
				throw std::runtime_error("expected 'end " + sysname() + "', not 'end '" + what + "'");
			}
		} else {				
			if(what.empty() || what == "foreach") {
				if(sysname() == "item_prefix") {
					throw std::runtime_error("foreach without <% item %>");
				} else {
					endline_ = line;
					return parent()->parent(); // this <- foreach_t <- foreach parent
				}
			} else {
				throw std::runtime_error("expected 'end foreach', not 'end '" + what + "'");
			}
		}
	}


	has_children_ptr foreach_t::prefix(file_position_t line) {
		if(!item_prefix_)
			item_prefix_ = std::make_shared<part_t>(line, "item_prefix", false, shared_from_this());
		return item_prefix_;
	}
	
	has_children_ptr foreach_t::suffix(file_position_t line) {
		if(!item_suffix_)
			item_suffix_ = std::make_shared<part_t>(line, "item_suffix", false, shared_from_this());
		return item_suffix_;
	}
	
	has_children_ptr foreach_t::empty(file_position_t line) {
		if(!empty_)
			empty_ = std::make_shared<part_t>(line, "item_empty", false, shared_from_this());
		return empty_;
	}
	has_children_ptr foreach_t::separator(file_position_t line) {
		if(!separator_)
			separator_ = std::make_shared<part_t>(line, "item_separator", false, shared_from_this());
		return separator_;
	}
	has_children_ptr foreach_t::item(file_position_t line) {
		if(!item_)
			item_ = std::make_shared<part_t>(line, "item", true, shared_from_this());
		return item_;
	}
		
	cache_t::cache_t(	file_position_t line, const expr::ptr& name, const expr::variable& miss, 
				int duration, bool recording, bool triggers, base_ptr parent) 
		: has_children("cache", line, true, parent) 
		, name_(name)
		, miss_(miss)
		, duration_(duration)
		, recording_(recording)
		, triggers_(triggers) {}
		
	base_ptr cache_t::add_trigger(file_position_t line, const expr::ptr& t) {
		trigger_list_.emplace_back(trigger_t { line, t });
		return shared_from_this();
	}
	
	void cache_t::dump(std::ostream& o, int tabs)  const {
		const std::string p(tabs, '\t');
		o << p << "cache " << *name_;
		if(duration_ > -1)
			o << " (cached for " << duration_ << "s)";
		if(miss_)
			o << " (call " << *miss_ << " on miss)";
		o << " recording is " << (recording_ ? "ON" : "OFF") << 
			" and triggers are " << (triggers_ ? "ON" : "OFF");
		if(trigger_list_.empty()) {
			o << " - no triggers\n";
		} else { 
			o << " - triggers [\n";
			for(const trigger_t& trigger : trigger_list_)
				o << p << "\t" << *trigger.ptr << std::endl;
			o << p << "]\n";
		}
		o << p << "cache children = [\n";
		for(const base_ptr& child : children) {
			child->dump(o, tabs+1);
		}
		o << p << "]\n";
	}
	
	void cache_t::write(generator::context& context, std::ostream& o) {
		o << ln(line()) << "{\n" << "std::string _cppcms_temp_val;\n";
		o << ln(line()) << "\tif (content.app().cache().fetch_frame(" << name_->code(context) << ", _cppcms_temp_val))\n";
		o << ln(line()) << "\t\tout() << _cppcms_temp_val;\n";
		o << ln(line()) << "\telse {";
		o << ln(line()) << "\t\tcppcms::copy_filter _cppcms_cache_flt(out());\n";
		if(recording_) {
			o << ln(line()) << "\t\tcppcms::triggers_recorder _cppcms_trig_rec(content.app().cache());\n";
		}
		if(miss_) {
			o << ln(line()) << "\t\t" << miss_->code(context) << ";\n";
		}
		has_children::write(context, o);
		o << ln(endline()) << "content.app().cache().store_frame(" << name_->code(context) << ", _cppcms_cache_flt.detach(),";
		if(recording_)
			o << "_cppcms_trig_rec.detach(),";
		else
			o << "std::set <std::string > (),";
		o << duration_ << ", " << (triggers_ ? "false" : "true")  << ");\n";
		o << ln(endline()) << "\t}} // cache\n";

	}
	
	base_ptr cache_t::end(const std::string& what,file_position_t line) {
		if(what.empty() || what == "cache") {
			endline_ = line;
			return parent();
		} else {
			throw std::runtime_error("expected 'end cache', not 'end '" + what + "'");
		}
	}	
}}}
