#include <cppcms/form.h>
#include <cppcms/themed_form.h>

class test_form : public cppcms::themed_form {
public:
	cppcms::widgets::themed::text name;
	cppcms::widgets::themed::hidden hidden;
	cppcms::widgets::themed::textarea textarea;
	cppcms::widgets::themed::numeric<int> numeric;
	cppcms::widgets::themed::password password;
	cppcms::widgets::themed::regex_field regex;
	cppcms::widgets::themed::email email;
	cppcms::widgets::themed::checkbox checkbox;;
	cppcms::widgets::themed::select_multiple select_multiple;
	cppcms::widgets::themed::select select;
	cppcms::widgets::themed::file file;
	cppcms::widgets::themed::radio radio;
	cppcms::widgets::themed::submit submit;

	test_form() {
		add_field(name);
		add_field(hidden);
		add_field(textarea);
		add_field(numeric);
		add_field(password);
		add_field(regex);
		add_field(email);
		add_field(checkbox);
		add_field(select_multiple);
		add_field(select);
		add_field(file);
		add_field(radio);
		add_action(submit);
	}
};

namespace data {
	class test {
		test_form formdata;
	};
}
