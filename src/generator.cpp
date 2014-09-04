#include "generator.h"
namespace cppcms { namespace templates { namespace generator {
	void context::add_scope_variable(const std::string& name) {
		if(!scope_variables.insert(name).second)
			throw std::runtime_error("duplicate local scope variable: " + name);
	}
	
	void context::remove_scope_variable(const std::string& name) {
		if(!scope_variables.erase(name))
			throw std::logic_error("bug: tried to remove variable: " + name + " which is notin local scope");
	}
		
	bool context::check_scope_variable(const std::string& name) {
		return scope_variables.find(name) != scope_variables.end();
	}
	
	void context::add_include(const std::string& filename) {
		includes.insert(filename);
	}

}}}
