#include "expr.h"
#include <algorithm>
#include <boost/lexical_cast.hpp>
namespace cppcms { namespace templates { namespace expr {
	static std::string trim(const std::string& input) {
		size_t beg = 0, end = input.length();
		
		while(beg < input.length() && std::isspace(input[beg]))
			++beg;

		while(end > beg && std::isspace(input[end-1]))
			--end;
		return std::string(&input[beg], &input[end]);
	}
	
	static std::string decode_escaped_string(const std::string& input) {
		std::string result;
		bool escaped = false;
		const int hex[256] = 
			{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
			  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
			  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
			  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, -1, -1, -1, -1, -1, -1, 
			  -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
			  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
			  -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
			  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
			  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
			  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
			  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
			  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
			  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
			  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
			  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
			  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
		for(size_t i = 0; i < input.length(); ++i) {
			char current = input[i];
			if(escaped) {
				escaped = false;
				int value;
				char o1,o2,o3;
				switch(current) {					
					case '\'':
					case '?':
					case '\\':
						result += current;
						break;
					case '"': result += "\\\""; break;
					case 'a': result += '\a'; break;
					case 'b': result += '\b'; break;
					case 'f': result += '\f'; break;
					case 'n': result += '\n'; break;
					case 'r': result += '\r'; break;
					case 't': result += '\t'; break;
					case 'v': result += '\v'; break;
							  							  
					case 'x':
						  if(i+2 >= input.length())
							  throw std::runtime_error("Invalid escape sequence: " + std::string(&input[i-1], &input[input.length()-1]));
						  value = hex[static_cast<int>(input[i+1])]*16 + hex[static_cast<int>(input[i+2])];
						  if(value < 0)
							  throw std::runtime_error("Invalid escape sequence: " + std::string(&input[i-1], &input[i+3]));
						  result += static_cast<char>(value);
						  break;
					// TODO: unicode characters
					default:
						if(input[i] >= '0' && input[i] <= '7') {
							if(i+3 >= input.length())
								throw std::runtime_error("Invalid escape sequence: " + std::string(&input[i-1], &input[input.length()-1]));
							o1 = input[i+1], o2 = input[i+2], o3 = input[i+3];
							if(o2 >= '0' && o2 <= '7' && o3 >= '0' && o3 <= '7') {
								o1 -= '0'; o2 -= '0'; o3 -= '0';
								result += static_cast<char>(o1*64+o2*8+o3);
							} else {
								throw std::runtime_error("Invalid escape sequence: " + std::string(&input[i-1], &input[i+4]));
							}
						} else {
							result += '\\';
							result += current;
						}
						break;
				}
			} else if(input[i] == '\\') {
				escaped = true;
			} else {
				result += current;
			}
		}
		if(escaped)
			result += '\\';
		return result;
	}
	
	static std::string compress_html(const std::string& input) {
		std::string result;
		char translate[255] = { 0 };
		translate[static_cast<unsigned int>('\a')] = 'a';
		translate[static_cast<unsigned int>('\b')] = 'b'; 
		translate[static_cast<unsigned int>('\f')] = 'f';
		translate[static_cast<unsigned int>('\n')] = 'n';
		translate[static_cast<unsigned int>('\r')] = 'r';
		translate[static_cast<unsigned int>('\t')] = 't';
		translate[static_cast<unsigned int>('\v')] = 'v';
		for(const char c : input) {
			if(c == '"') {
				result += "\\\"";
			} else if(c == '\\') {
				result += '\\';
				result += c;
			} else if(translate[static_cast<unsigned char>(c)] > 0) {
				result += '\\';
				result += translate[static_cast<unsigned char>(c)];
			} else { // if(std::isprint(c)) {
				result += c;
			}
		}
		return result;
	}
	
	static std::string compress_string(const std::string& input) {
		std::string result = "\"";
		char translate[255] = { 0 };
		translate[static_cast<unsigned int>('\a')] = 'a';
		translate[static_cast<unsigned int>('\b')] = 'b'; 
		translate[static_cast<unsigned int>('\f')] = 'f';
		translate[static_cast<unsigned int>('\n')] = 'n';
		translate[static_cast<unsigned int>('\r')] = 'r';
		translate[static_cast<unsigned int>('\t')] = 't';
		translate[static_cast<unsigned int>('\v')] = 'v';
		for(size_t i = 1; i < input.length()-1; ++i) {
			const char c = input[i];
			if(c == '\\' && input[i+1] == '"') {
				result += "\\\"";
				++i;
			} else if(c == '\\') {
				result += '\\';
				result += c;
			} else if(translate[static_cast<unsigned char>(c)] > 0) {
				result += '\\';
				result += translate[static_cast<unsigned char>(c)];
			} else { // if(std::isprint(c)) {
				result += c;
			}
		}
		result += '"';
		return result;		
	}
	
	static expr::ptr recognize_expr(const std::string& input) {
		const std::string trimmed = trim(input);
		if(trimmed[0] == '"')
			return expr::make_string(trimmed);
		else if(trimmed.length() >= 3 && trimmed[0] == '0' && trimmed[1] == 'x')
			return expr::make_number(trimmed);
		else if(std::all_of(trimmed.begin(), trimmed.end(), [](char c) {
			return c == '-' || c == '.' || c == '+' || (c >= '0' && c <= '9');
		})) {
			return expr::make_number(trimmed);
		} else
			return expr::make_variable(trimmed);
	}

	static std::pair<std::string, std::vector<expr::ptr>> split_function_call(const std::string& call) {
		typedef std::pair<std::string, std::vector<expr::ptr>> result_t;
		result_t result;
		size_t beg = call.find("("), end = call.length();
		if(beg == std::string::npos) {
			result.first = call;
			return result;
		} else {
			result.first = std::string(&call[0], &call[beg]);
			// parse c++ expression
			// bracket counts, a = (), b = [], c = <>
			int brackets_a = 0, brackets_b = 0, brackets_c = 0;
			// in string
			bool string = false, escaped = false;
			size_t next = beg+1;

			for(size_t i = beg+1; i < end-1; ++i) {
				const char c = call[i];
				if((brackets_a == 0 && brackets_b == 0 && brackets_c == 0 
					&& !string && c == ',') || i == end-1) {
					const std::string arg(&call[next], &call[i]);					
					result.second.emplace_back(recognize_expr(arg));
					next = i+1;
				} else if(c == '(' && !string) 
					brackets_a++;
				else if(c == ')' && !string)
					brackets_a--;
				else if(c == '[' && !string)
					brackets_b++;
				else if(c == ']' && !string)
					brackets_b--;
				else if(c == '<' && !string)
					brackets_c++;
				else if(c == '>' && !string)
					brackets_c--;
				else if(c == '"' && !string) 
					string = true;
				else if(c == '"' && string && !escaped)
					string = false;
				else if(c == '\\' && string && !escaped)
					escaped = true;
				else {
					escaped = false;
				}
			}
			result.second.emplace_back(recognize_expr(std::string(&call[next], &call[end-1])));
			return result;
		}
	}

	base_t::base_t(const std::string& value)
		: value_(value) {}

	std::string text_t::repr() const { 
		return "\"" + value_ + "\"";
	}

	std::string text_t::code(generator::context&) const {
		return "\"" + value_ + "\"";
	}

	double number_t::real() const {
		return boost::lexical_cast<double>(value_);
	}

	int number_t::integer() const {
		return boost::lexical_cast<int>(value_);
	}

	std::string number_t::repr() const {
		return value_;
	}
	
	std::string number_t::code(generator::context&) const {
		return value_;
	}

	variable_t::variable_t(const std::string& input, bool consume_all, size_t* pos) 
		: base_t(input) {
		size_t index = 0;
		size_t& i = ( pos == nullptr ? index : *pos );
		
		for(; i < input.length() && std::isspace(input[i]); ++i);

		if(input[i] == '*') {
			is_deref = true;
			++i;
		} else {
			is_deref = false;
		}
		
		for(; i < input.length() && std::isspace(input[i]); ++i);

		std::string name;
		std::vector<ptr> arguments;
		bool function = false;
		for(;i < input.length(); ++i) {
			const char c = input[i];
			if(c == '.') { // separator: .
				parts.push_back({ name, arguments, ".", function });
				arguments.clear();
				function = false;
				name.clear();
			} else if(c == '-' && i < input.length()-1 && input[i+1] == '>') { // sperator: ->
				++i;
				parts.push_back({ name, arguments, "->", function });
				arguments.clear();
				function = false;
				name.clear();
			} else if(c == '(') {
				arguments = parse_arguments(input, i);
				--i;
				function = true;
			} else if(std::isspace(c)) {
				break;
			} else if(c == ',' || c == ')') {
				break;
			} else {
				name += c;
			}
		}

		if(!name.empty()) {
			parts.push_back({ name, arguments, "", function });
		}
		
		for(; i < input.length() && std::isspace(input[i]); ++i);

		if(consume_all && i != input.length()) {
			throw std::runtime_error("Parse error at variable expression, characters left: " + input.substr(i));
		}
	}

	std::vector<ptr> variable_t::parse_arguments(const std::string& input, size_t& i) {
		++i;
		// clear whitespace between '(' and next token
		for(; i < input.length() && std::isspace(input[i]); ++i);
		std::vector<ptr> arguments;
		ptr tmp;
		bool separated = true;
		for(; i < input.length(); ++i) {
			const char c = input[i];
			const bool has_next = (i < input.length()-1);
			const char next = (has_next ? input[i+1] : 0);
			if(separated && c == '"') {
				tmp = parse_string(input, i);
				--i;
				arguments.push_back(tmp);
				separated = false;
			} else if( separated && (
					(c == '-' && has_next && next >= '0' && next <= '9') ||
					(c >= '0' && c <= '9')
					) 
				) {
				tmp = parse_number(input, i);
				--i;
				arguments.push_back(tmp);
				separated = false;
			} else if(std::isspace(c)) {
			} else if(c == ',') {
				separated = true;
			} else if(c == ')') {
				break;
			} else if(separated) {
				tmp = std::make_shared<variable_t>(input, false, &i);
				--i;
				arguments.push_back(tmp);
				separated = false;
			} else {
				throw std::runtime_error("argument is neither string, variable or number: " + input.substr(i));
			}
		}
		if(i < input.length() && input[i] == ')') {
			++i;
			return arguments;
		} else {
			throw std::runtime_error("unterminated argument list");
		}
	}

	ptr variable_t::parse_string(const std::string& input, size_t& i) {
		bool escaped = false;
		size_t start = i;
		++i;
		for(;i < input.length(); ++i) {
			const char c = input[i];

			if(c == '"' && !escaped) {
				break;
			} else if(c == '\\' && !escaped) {
				escaped = true;
			} else {
				escaped = false;
			}
		}
		if(i < input.length() && input[i] == '"') {
			++i;
			return make_string(input.substr(start, i-start));
		} else {
			throw std::runtime_error("unterminated string");
		}
	}

	ptr variable_t::parse_number(const std::string& input, size_t& i) {
		size_t start = i;
		bool oct = false;
		bool hex = false;
		bool dot = false;
		if(input[i] == '-' || input[i] == '+')
			++i;

		if(i < input.length()-2 && input[i] == '0' && input[i+1] == 'x') {
			i += 2;
			hex = true;
		} else if(input[i] == '0') {
			oct = true;
		}

		for(;i < input.length(); ++i) {
			const char c = input[i];
			if( (c >= '0' && c <= '7') ||
				(!oct && c >= '8' && c <= '9') ||
				(hex && c >= 'a' && c <= 'f') ||
				(hex && c >= 'A' && c <= 'F')) {					
			} else if(!dot && c == '.') {
				dot = true;
			} else {
				break;
			}
		}
		return make_number(input.substr(start, i-start));					
	}

	std::string variable_t::repr() const {
		return value_;
	}
	

	const std::pair<std::string, bool> split_exp_filter(const std::string& input) {
		const std::string ext("ext ");
		if(input.compare(0, ext.length(), ext) == 0) {
			return std::make_pair(input.substr(ext.length()), true);
		} else {
			return std::make_pair(input, false);
		}
	}
	filter_t::filter_t(const std::string& input) 
		: call_list_t(split_exp_filter(input).first, 
				split_exp_filter(input).second ? "$var" : "cppcms::filters::")
		, exp_(split_exp_filter(input).second) {}
			
	bool filter_t::is_exp() const { return exp_; }

	std::string filter_t::code(generator::context& context) const {
		if(exp_) {
			return call_list_t::code(context);
		} else {
			return call_list_t::code(context);
		}
	}

	std::string call_list_t::code(generator::context& context) const {
		std::ostringstream oss;
		if(function_prefix_ == "$var")
			oss << context.variable_prefix << value_ << "(  ";
		else
			oss << function_prefix_ << value_ << "(  ";
		if(!current_argument_.empty())
			oss << current_argument_ << ", ";
		for(const ptr& x : arguments_) {
			if(x->is_a<expr::variable_t>())
				oss << x->as<expr::variable_t>().code(context) << ", ";
			else
				oss << x->code(context) << ", ";
		}
		const std::string result = oss.str();
		return result.substr(0, result.length()-2) + ")";
	}

	std::string variable_t::code(generator::context& context) const {
		std::ostringstream o;
		if(is_deref) { 
			o << "*";
		}

		bool first = true;
		for(const part_t& part : parts) {
			if(!first || context.check_scope_variable(part.name)) {
				o << part.name;
			} else {
				o << context.variable_prefix << part.name;
			}

			first = false;

			if(part.is_function) {
				o << "(";
				for(auto i = part.arguments.begin(); i != part.arguments.end(); ++i) {
					if(i != part.arguments.begin())
						o << ", ";
					o << (*i)->code(context);
				}
				o << ")";
			}
			o << part.separator;
		}

		return o.str();
	}

	std::string string_t::repr() const { 
		return value_;
	}
	
	std::string string_t::code(generator::context&) const {
		return value_;
	}

	
	std::string cpp_t::repr() const { 
		return value_;
	}
	
	std::string cpp_t::code(generator::context&) const { 
		return value_;
	}
	

	call_list_t::call_list_t(const std::string& value, const std::string& function_prefix)
		: base_t(split_function_call(value).first)
		, arguments_(split_function_call(value).second) 
		, function_prefix_(function_prefix) {}

	std::string call_list_t::repr() const { 
		std::string result = value_ + "(";
		for(const ptr& x : arguments_)
			result += x->repr() + ",";
		result[result.length()-1] = ')';
		return result;
	}

	call_list_t& call_list_t::argument(const std::string& arg) {
		current_argument_ = arg;
		return *this;
	}
		
	std::string string_t::unescaped() const {
		return decode_escaped_string(value_);
	}
		
	string_t::string_t(const std::string& in) 
		: base_t(compress_string(in)) {}

	std::string name_t::repr() const {
		return value_;
	}
	
	std::string name_t::code(generator::context&) const {
		return value_;
	}
	
	std::string identifier_t::repr() const {
		return value_;
	}
		
	std::string identifier_t::code(generator::context&) const {
		return value_;
	}
	
	param_list_t::param_list_t(const std::string& input, const params_t& params)
		: base_t(trim(input)) 
		, params_(params) {}

	const param_list_t::params_t& param_list_t::params() const { return params_; }

	std::string param_list_t::repr() const {
		return value_;
	}
	
	std::string param_list_t::code(generator::context&) const {
		return value_;
	}

	bool name_t::operator<(const name_t& rhs) const {
		return repr() < rhs.repr();
	}
	
	html make_html(const std::string& repr) {
		return std::make_shared<html_t>(compress_html(repr));
	}
	
	xhtml make_xhtml(const std::string& repr) {
		return std::make_shared<xhtml_t>(compress_html(repr));
	}
	
	text make_text(const std::string& repr) {
		return std::make_shared<text_t>(compress_html(repr));
	}
	
	number make_number(const std::string& repr) {
		return std::make_shared<number_t>(repr);
	}

	variable make_variable(const std::string& repr) {
		return std::make_shared<variable_t>(repr);
	}
	
	filter make_filter(const std::string& repr) {
		return std::make_shared<filter_t>(repr);
	}

	string make_string(const std::string& repr) {
		return std::make_shared<string_t>(repr);
	}

	name make_name(const std::string& repr) {
		return std::make_shared<name_t>(repr);
	}
	
	cpp make_cpp(const std::string& repr) {
		return std::make_shared<cpp_t>(repr);
	}
	
	identifier make_identifier(const std::string& repr) {
		return std::make_shared<identifier_t>(repr);
	}
	
	call_list make_call_list(const std::string& repr, const std::string& prefix) {
		return std::make_shared<call_list_t>(repr, prefix);
	}
	
	param_list make_param_list(const std::string& repr, const param_list_t::params_t& params) {
		return std::make_shared<param_list_t>(repr, params);
	}
	
	std::ostream& operator<<(std::ostream& o, const name_t& obj) {
		return o << "[name:" << obj.repr() << "]";
	}
	
	std::ostream& operator<<(std::ostream& o, const string_t& obj) {
		return o << "[string:" << obj.repr() << "]";
	}
	
	std::ostream& operator<<(std::ostream& o, const identifier_t& obj) {
		return o << "[id:" << obj.repr() << "]";
	}
	
	std::ostream& operator<<(std::ostream& o, const call_list_t& obj) {
		return o << "[calllist:" << obj.repr() << "]";
	}
	
	std::ostream& operator<<(std::ostream& o, const param_list_t& obj) {
		return o << "[paramlist:" << obj.repr() << "]";
	}
	
	std::ostream& operator<<(std::ostream& o, const filter_t& obj) {
		return o << "[filter:" << obj.repr() << "]";
	}
	
	std::ostream& operator<<(std::ostream& o, const variable_t& obj) {
		return o << "[variable:" << obj.repr() << "]";
	}
	
	std::ostream& operator<<(std::ostream& o, const cpp_t& obj) {
		return o << "[cpp:" << obj.repr() << "]";
	}

	std::ostream& operator<<(std::ostream& o, const text_t& obj) {
		return o << "[text:" << obj.repr() << "]";
	}

	std::ostream& operator<<(std::ostream& o, const html_t& obj) {
		return o << "[html:" << obj.repr() << "]";
	}

	std::ostream& operator<<(std::ostream& o, const xhtml_t& obj) {
		return o << "[xhtml:" << obj.repr() << "]";
	}
	
	std::ostream& operator<<(std::ostream& o, const base_t& obj) {
		// currently only usefull types are detected
		o << "[autodetect:";
		if(obj.is_a<number_t>())
			o << obj.as<number_t>();
		else if(obj.is_a<variable_t>())
			o << obj.as<variable_t>();
		else if(obj.is_a<string_t>())
			o << obj.as<string_t>();
		else if(obj.is_a<html_t>())
			o << obj.as<html_t>();
		else if(obj.is_a<text_t>())
			o << obj.as<text_t>();
		else if(obj.is_a<xhtml_t>())
			o << obj.as<xhtml_t>();
		else
			throw std::logic_error(std::string("could not autodetect type ") + typeid(obj).name());
		o << "]";
		return o;
	}
}}}
