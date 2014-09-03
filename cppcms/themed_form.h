#ifndef CPPCMS_THEMED_FORM_H
#define CPPCMS_THEMED_FORM_H
#include <cppcms/form.h>
#include <cppcms/filters.h>
#include <cppcms/view.h>

namespace cppcms { namespace widgets { namespace themed {
	template<typename T>
	struct wrap : public T {
		using T::T;
	};

	template<>
	struct wrap<cppcms::widgets::select> : public cppcms::widgets::select {
		struct element_t {
			const std::string id, title;
			const bool selected;
		};

		std::vector<element_t> elements() {
			std::vector<element_t> result;
			for(size_t i = 0;i < elements_.size(); ++i) {
				const element& el = elements_[i];

				bool is_selected = ( selected() == i );
				if(el.need_translation) { // FIXME: something about translation
					result.push_back( { cppcms::util::escape(el.id), cppcms::util::escape(el.tr_option), is_selected } );
				} else {
					result.push_back( { cppcms::util::escape(el.id), cppcms::util::escape(el.str_option), is_selected } );
				}
			}
			return result;
		}
	};

	template<>
	struct wrap<cppcms::widgets::radio> : public cppcms::widgets::radio {
		struct element_t {
			const std::string id, title;
			const bool selected;
		};

		std::vector<element_t> elements() {
			std::vector<element_t> result;
			for(size_t i = 0;i < elements_.size(); ++i) {
				const element& el = elements_[i];

				bool is_selected = ( selected() == i );  
				if(el.need_translation) { // FIXME: something about translation
					result.push_back( { cppcms::util::escape(el.id), cppcms::util::escape(el.tr_option), is_selected } );
				} else {
					result.push_back( { cppcms::util::escape(el.id), cppcms::util::escape(el.str_option), is_selected } );
				}
			}
			return result;
		}
	};

	template<>
	struct wrap<cppcms::widgets::select_multiple> : public cppcms::widgets::select_multiple {
		struct element_t {
			const std::string id, title;
			const bool selected;
		};

		std::vector<element_t> elements() {
			throw std::logic_error("Not implemented.");
			// TODO: elements are not accessible
			/*
			std::vector<element_t> result;
			for(size_t i = 0;i < elements_.size(); ++i) {
				const element& el = elements_[i];

				bool is_selected = el.selected;
				if(el.need_translation) { // FIXME: something about translation
					result.push_back( { cppcms::util::escape(el.id), cppcms::util::escape(el.tr_option), is_selected } );
				} else {
					result.push_back( { cppcms::util::escape(el.id), cppcms::util::escape(el.str_option), is_selected } );
				}
			}
			return result;
			*/
		}
	};


	typedef cppcms::widgets::base_widget base;
	typedef wrap<cppcms::widgets::text> text;
	typedef wrap<cppcms::widgets::hidden> hidden;
	typedef wrap<cppcms::widgets::textarea> textarea;

	template<typename T>
	using numeric = wrap<cppcms::widgets::numeric<T>>;
	typedef wrap<cppcms::widgets::password> password;
	typedef wrap<cppcms::widgets::regex_field> regex_field;
	typedef wrap<cppcms::widgets::email> email;
	typedef wrap<cppcms::widgets::checkbox> checkbox;
	typedef wrap<cppcms::widgets::select_multiple> select_multiple;
	typedef wrap<cppcms::widgets::select> select;
	typedef wrap<cppcms::widgets::file> file;
	typedef wrap<cppcms::widgets::submit> submit;
	typedef wrap<cppcms::widgets::radio> radio;
}}}

namespace cppcms {
	class themed_form : public form {
		typedef std::vector<widgets::base_widget*> widgets_t;
		widgets_t fields_, actions_;
	public:
		using form::form;
		const widgets_t& fields() const { return fields_; }
		const widgets_t& actions() const { return actions_; }
		template<typename T>
		T& add_field(T& widget) { fields_.push_back(&widget); return *fields_.back(); }
		
		template<typename T>
		T& add_action(T& widget) { actions_.push_back(&widget); return *actions_.back(); }
	};
}

#endif
