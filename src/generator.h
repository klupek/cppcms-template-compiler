#ifndef CPPCMS_TEMPLATE_COMPILER_GENERATOR_H
#define CPPCMS_TEMPLATE_COMPILER_GENERATOR_H
#include <vector>
#include <string>
#include <set>
#include <map>

namespace cppcms { namespace templates { namespace generator {
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
		private:
			std::set<std::string> scope_variables;

		};
	}
}}	

#endif
