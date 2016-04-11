/*
    Copyright (c) 2011-2015 Anton Matveenko <antmat@yandex-team.ru>
    Copyright (c) 2011-2015 Other contributors as noted in the AUTHORS file.

    This file is part of Cocaine.

    Cocaine is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    Cocaine is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "cocaine/logging/filter.hpp"

#include <cocaine/errors.hpp>
#include <cocaine/format.hpp>

#include <boost/optional/optional.hpp>

namespace cocaine {
namespace logging {

namespace {

inline boost::optional<const attribute_t&> find_attribute(const attributes_t& attributes,
                                                          const std::string& attribute_name) {
    auto it = std::find_if(attributes.begin(), attributes.end(), [&](const attribute_t& attr) {
        return attr.name == attribute_name;
    });
    return it == attributes.end() ? boost::none : boost::optional<const attribute_t&>(*it);
}

}

filter_info_t::filter_info_t(filter_t _filter,
                             filter_t::deadline_t _deadline,
                             filter_t::id_type _id,
                             filter_t::disposition_t _disposition,
                             std::string _logger_name)
    : filter(std::move(_filter)),
      deadline(std::move(_deadline)),
      id(_id),
      disposition(_disposition),
      logger_name(std::move(_logger_name)) {}

filter_info_t::filter_info_t(const dynamic_t& value) {
    if (!value.is_object()) {
        throw cocaine::error_t("invalid representation for filter info - %s",
                               boost::lexical_cast<std::string>(value));
    }
    const auto& obj = value.as_object();
    if (!obj.count("filter") || !obj.count("deadline") || !obj.count("id") ||
        !obj.count("logger_name")) {
        throw cocaine::error_t("invalid representation for filter info - %s",
                               boost::lexical_cast<std::string>(value));
    }
    filter = filter_t(obj.at("filter"));
    typedef filter_t::deadline_t dl_t;
    deadline = dl_t(dl_t::duration(obj.at("deadline").as_uint()));
    id = obj.at("id").as_uint();
    logger_name = obj.at("logger_name").as_string();
}

dynamic_t filter_info_t::representation() {
    dynamic_t::object_t container;
    container["filter"] = filter.representation();
    container["deadline"] = deadline.time_since_epoch().count();
    container["id"] = id;
    container["logger_name"] = logger_name;
    return dynamic_t(std::move(container));
}

class filter_t::inner_t {
public:
    virtual ~inner_t() {}

    virtual filter_result_t apply(const std::string& message,
                                  unsigned int severity,
                                  const logging::attributes_t& attributes) const = 0;

    virtual dynamic_t representation() const = 0;
};

void filter_t::deleter_t::operator()(filter_t::inner_t* ptr) {
    delete ptr;
}

typedef std::unique_ptr<filter_t::inner_t, filter_t::deleter_t> inner_t;

namespace {

typedef filter_result_t fr;

template <template <class> class Filter>
struct filter_creation_visitor_t {
    typedef inner_t::pointer result_type;

    filter_creation_visitor_t(std::string _attribute_name)
        : attribute_name(std::move(_attribute_name)) {}

    result_type operator()(dynamic_t::bool_t value) {
        return new Filter<dynamic_t::bool_t>(std::move(attribute_name), value);
    }

    result_type operator()(dynamic_t::uint_t value) {
        return new Filter<dynamic_t::uint_t>(std::move(attribute_name), value);
    }

    result_type operator()(dynamic_t::int_t value) {
        return new Filter<dynamic_t::int_t>(std::move(attribute_name), value);
    }

    result_type operator()(dynamic_t::double_t value) {
        return new Filter<dynamic_t::double_t>(std::move(attribute_name), value);
    }

    result_type operator()(dynamic_t::string_t value) {
        return new Filter<std::string>(std::move(attribute_name), std::move(value));
    }

    template <class T>
    result_type operator()(T&&) {
        throw std::system_error(std::make_error_code(std::errc::function_not_supported),
                                "invalid representation - cannot create filter from value");
    }

    std::string attribute_name;
};

struct null_filter_t : public filter_t::inner_t {
    virtual filter_result_t apply(const std::string&,
                                  unsigned int,
                                  const logging::attributes_t&) const {
        throw std::system_error(std::make_error_code(std::errc::operation_not_supported),
                                "tried to apply null filter");
    }

    virtual dynamic_t representation() const {
        throw std::system_error(std::make_error_code(std::errc::operation_not_supported),
                                "tried to get representation of null filter");
    };
};

struct empty_filter_t : public filter_t::inner_t {
    virtual filter_result_t apply(const std::string&,
                                  unsigned int,
                                  const logging::attributes_t&) const {
        return fr::accept;
    }

    virtual dynamic_t representation() const {
        return dynamic_t(std::make_tuple(std::string("empty")));
    };
};

struct severity_filter_t : public filter_t::inner_t {
    unsigned int ref_severity;

    severity_filter_t(unsigned int severity) : ref_severity(severity) {}

    virtual filter_result_t apply(const std::string&,
                                  unsigned int severity,
                                  const logging::attributes_t&) const {
        return severity >= ref_severity ? fr::accept : fr::reject;
    }

    virtual dynamic_t representation() const {
        return dynamic_t(std::make_tuple(std::string("severity"), ref_severity));
    };
};

struct exists_filter_t : public filter_t::inner_t {
    exists_filter_t(std::string _attribute_name) : attribute_name(std::move(_attribute_name)) {}

    virtual filter_result_t apply(const std::string&,
                                  unsigned int,
                                  const logging::attributes_t& attributes) const {
        return static_cast<bool>(logging::find_attribute(attributes, attribute_name)) ? fr::accept
                                                                                      : fr::reject;
    }

    virtual dynamic_t representation() const {
        return dynamic_t(std::make_tuple(std::string("e"), attribute_name));
    };

    std::string attribute_name;
};

struct not_exists_filter_t : public filter_t::inner_t {
    not_exists_filter_t(std::string _attribute_name) : attribute_name(std::move(_attribute_name)) {}

    virtual filter_result_t apply(const std::string&,
                                  unsigned int,
                                  const logging::attributes_t& attributes) const {
        return static_cast<bool>(logging::find_attribute(attributes, attribute_name)) ? fr::reject
                                                                                      : fr::accept;
    }

    virtual dynamic_t representation() const {
        return dynamic_t(std::make_tuple(std::string("!e"), attribute_name));
    };

    std::string attribute_name;
};

template <class T>
struct comparision_filter_t : public filter_t::inner_t {
    comparision_filter_t(std::string _attribute_name, T _attribute_value)
        : attribute_name(std::move(_attribute_name)),
          attribute_value(std::move(_attribute_value)) {}

    std::string attribute_name;
    T attribute_value;
};

template <class T>
struct equals_filter_t : public comparision_filter_t<T> {
    equals_filter_t(std::string _attribute_name, T _attribute_value)
        : comparision_filter_t<T>(std::move(_attribute_name), std::move(_attribute_value)) {}

    virtual filter_result_t apply(const std::string&,
                                  unsigned int,
                                  const logging::attributes_t& attributes) const {
        const auto& val = logging::find_attribute(attributes, this->attribute_name);
        return (val && val->value.template convertible_to<T>() &&
                val->value.template to<T>() == this->attribute_value)
                   ? fr::accept
                   : fr::reject;
    }

    virtual dynamic_t representation() const {
        return dynamic_t(
            std::make_tuple(std::string("=="), this->attribute_name, this->attribute_value));
    };
};

template <class T>
struct not_equals_filter_t : public comparision_filter_t<T> {
    not_equals_filter_t(std::string _attribute_name, T _attribute_value)
        : comparision_filter_t<T>(std::move(_attribute_name), std::move(_attribute_value)) {}

    virtual filter_result_t apply(const std::string&,
                                  unsigned int,
                                  const logging::attributes_t& attributes) const {
        const auto& val = logging::find_attribute(attributes, this->attribute_name);
        return (val && val->value.template convertible_to<T>() &&
                val->value.template to<T>() == this->attribute_value)
                   ? fr::reject
                   : fr::accept;
    }

    virtual dynamic_t representation() const {
        return dynamic_t(
            std::make_tuple(std::string("!="), this->attribute_name, this->attribute_value));
    };
};

template <class T>
struct greater_filter_t : public comparision_filter_t<T> {
    greater_filter_t(std::string _attribute_name, T _attribute_value)
        : comparision_filter_t<T>(std::move(_attribute_name), std::move(_attribute_value)) {}

    virtual filter_result_t apply(const std::string&,
                                  unsigned int,
                                  const logging::attributes_t& attributes) const {
        const auto& val = logging::find_attribute(attributes, this->attribute_name);
        return (val && val->value.template convertible_to<T>() &&
                val->value.template to<T>() > this->attribute_value)
                   ? fr::accept
                   : fr::reject;
    }

    virtual dynamic_t representation() const {
        return dynamic_t(
            std::make_tuple(std::string(">"), this->attribute_name, this->attribute_value));
    };
};

template <class T>
struct less_filter_t : public comparision_filter_t<T> {
    less_filter_t(std::string _attribute_name, T _attribute_value)
        : comparision_filter_t<T>(std::move(_attribute_name), std::move(_attribute_value)) {}

    virtual filter_result_t apply(const std::string&,
                                  unsigned int,
                                  const logging::attributes_t& attributes) const {
        const auto& val = logging::find_attribute(attributes, this->attribute_name);
        return (val && val->value.template convertible_to<T>() &&
                val->value.template to<T>() < this->attribute_value)
                   ? fr::accept
                   : fr::reject;
    }

    virtual dynamic_t representation() const {
        return dynamic_t(
            std::make_tuple(std::string("<"), this->attribute_name, this->attribute_value));
    };
};

template <class T>
struct greater_or_equal_filter_t : public comparision_filter_t<T> {
    greater_or_equal_filter_t(std::string _attribute_name, T _attribute_value)
        : comparision_filter_t<T>(std::move(_attribute_name), std::move(_attribute_value)) {}

    virtual filter_result_t apply(const std::string&,
                                  unsigned int,
                                  const logging::attributes_t& attributes) const {
        const auto& val = logging::find_attribute(attributes, this->attribute_name);
        return (val && val->value.template convertible_to<T>() &&
                val->value.template to<T>() >= this->attribute_value)
                   ? fr::accept
                   : fr::reject;
    }

    virtual dynamic_t representation() const {
        return dynamic_t(
            std::make_tuple(std::string(">="), this->attribute_name, this->attribute_value));
    };
};

template <class T>
struct less_or_equal_filter_t : public comparision_filter_t<T> {
    less_or_equal_filter_t(std::string _attribute_name, T _attribute_value)
        : comparision_filter_t<T>(std::move(_attribute_name), std::move(_attribute_value)) {}

    virtual filter_result_t apply(const std::string&,
                                  unsigned int,
                                  const logging::attributes_t& attributes) const {
        const auto& val = logging::find_attribute(attributes, this->attribute_name);
        return (val && val->value.template convertible_to<T>() &&
                val->value.template to<T>() <= this->attribute_value)
                   ? fr::accept
                   : fr::reject;
    }

    virtual dynamic_t representation() const {
        return dynamic_t(
            std::make_tuple(std::string("<="), this->attribute_name, this->attribute_value));
    };
};

struct or_filter_t : public filter_t::inner_t {
    filter_t filter1;
    filter_t filter2;

    or_filter_t(filter_t f1, filter_t f2) : filter1(std::move(f1)), filter2(std::move(f2)) {}

    virtual filter_result_t apply(const std::string& message,
                                  unsigned int severity,
                                  const logging::attributes_t& attributes) const {
        return (filter1.apply(message, severity, attributes) == fr::accept ||
                filter2.apply(message, severity, attributes) == fr::accept)
                   ? fr::accept
                   : fr::reject;
    }

    virtual dynamic_t representation() const {
        return dynamic_t(
            std::make_tuple(std::string("||"), filter1.representation(), filter2.representation()));
    };
};

struct and_filter_t : public filter_t::inner_t {
    filter_t filter1;
    filter_t filter2;

    and_filter_t(filter_t f1, filter_t f2) : filter1(std::move(f1)), filter2(std::move(f2)) {}

    virtual filter_result_t apply(const std::string& message,
                                  unsigned int severity,
                                  const logging::attributes_t& attributes) const {
        return (filter1.apply(message, severity, attributes) == fr::accept &&
                filter2.apply(message, severity, attributes) == fr::accept)
                   ? fr::accept
                   : fr::reject;
    }

    virtual dynamic_t representation() const {
        return dynamic_t(
            std::make_tuple(std::string("&&"), filter1.representation(), filter2.representation()));
    };
};

struct xor_filter_t : public filter_t::inner_t {
    filter_t filter1;
    filter_t filter2;

    xor_filter_t(filter_t f1, filter_t f2) : filter1(std::move(f1)), filter2(std::move(f2)) {}

    virtual filter_result_t apply(const std::string& message,
                                  unsigned int severity,
                                  const logging::attributes_t& attributes) const {
        return filter1.apply(message, severity, attributes) !=
                       filter2.apply(message, severity, attributes)
                   ? fr::accept
                   : fr::reject;
    }

    virtual dynamic_t representation() const {
        return dynamic_t(std::make_tuple(
            std::string("xor"), filter1.representation(), filter2.representation()));
    };
};

inner_t unary_operator_factory(const std::string& filter_operator, const dynamic_t& operand) {
    if (!operand.is_string()) {
        throw error_t(format("operand is not string for operator %s", filter_operator));
    } else if (filter_operator == "e") {
        return inner_t(new exists_filter_t(operand.as_string()));
    } else if (filter_operator == "!e") {
        return inner_t(new not_exists_filter_t(operand.as_string()));
    } else if (filter_operator == "severity") {
        return inner_t(new severity_filter_t(operand.as_uint()));
    }
    throw error_t(format("invalid unary filter operator: %s", filter_operator));
}

inner_t string_operand_factory(const std::string& filter_operator,
                               const dynamic_t& operand1,
                               const dynamic_t& operand2) {
    if (!operand1.is_string()) {
        throw error_t(format("operand 1 for operator %s should be strings", filter_operator));
    }
    if (filter_operator == "==") {
        filter_creation_visitor_t<equals_filter_t> visitor(operand1.as_string());
        return inner_t(operand2.apply(visitor));
    } else if (filter_operator == "!=") {
        filter_creation_visitor_t<not_equals_filter_t> visitor(operand1.as_string());
        return inner_t(operand2.apply(visitor));
    } else if (filter_operator == ">") {
        filter_creation_visitor_t<greater_filter_t> visitor(operand1.as_string());
        return inner_t(operand2.apply(visitor));
    } else if (filter_operator == "<") {
        filter_creation_visitor_t<less_filter_t> visitor(operand1.as_string());
        return inner_t(operand2.apply(visitor));
    } else if (filter_operator == ">=") {
        filter_creation_visitor_t<greater_or_equal_filter_t> visitor(operand1.as_string());
        return inner_t(operand2.apply(visitor));
    } else if (filter_operator == "<=") {
        filter_creation_visitor_t<less_or_equal_filter_t> visitor(operand1.as_string());
        return inner_t(operand2.apply(visitor));
    }
    throw std::logic_error(format("Invalid operator passed: %s", filter_operator));
}

inner_t filter_operand_factory(const std::string& filter_operator,
                               const dynamic_t& operand1,
                               const dynamic_t& operand2) {
    if (!operand1.is_array() || !operand2.is_array()) {
        throw error_t(format("operands for operator %s should be arrays", filter_operator));
    } else if (filter_operator == "||") {
        return inner_t(new or_filter_t(filter_t(operand1), filter_t(operand2)));
    } else if (filter_operator == "&&") {
        return inner_t(new and_filter_t(filter_t(operand1), filter_t(operand2)));
    } else if (filter_operator == "xor") {
        return inner_t(new xor_filter_t(filter_t(operand1), filter_t(operand2)));
    }
    throw std::logic_error(format("Invalid operator passed: %s", filter_operator));
}

inner_t binary_operator_factory(const std::string& filter_operator,
                                const dynamic_t& operand1,
                                const dynamic_t& operand2) {
    auto string_ops = {"==", "!=", ">", "<", ">=", "<=", "e", "!e"};
    if (std::count(string_ops.begin(), string_ops.end(), filter_operator)) {
        return string_operand_factory(filter_operator, operand1, operand2);
    } else {
        return filter_operand_factory(filter_operator, operand1, operand2);
    }
}
}

filter_t::filter_t() : inner(new null_filter_t()) {}

filter_result_t filter_t::apply(const std::string& message,
                                unsigned int severity,
                                const logging::attributes_t& attributes) const {
    return inner->apply(message, severity, attributes);
}

dynamic_t filter_t::representation() const {
    return inner->representation();
}

filter_t::filter_t(const dynamic_t& source) {
    if (!source.is_array()) {
        throw error_t("representation should be array, found - %s",
                      boost::lexical_cast<std::string>(source));
    }
    const auto& array = source.as_array();
    if (array.size() < 1) {
        throw error_t("representation should contain  at least 1 element");
    }
    if (!array[0].is_string()) {
        throw error_t("operator should be string");
    }
    const auto filter_operator = array[0].as_string();
    const auto operand1 = array[1];
    if (array.size() == 1 && array[0].is_string() && array[0].as_string() == "empty") {
        inner.reset(new empty_filter_t());
    } else if (array.size() == 2) {
        inner = unary_operator_factory(filter_operator, operand1);
    } else if (array.size() == 3) {
        const auto operand2 = array[2];
        inner = binary_operator_factory(filter_operator, operand1, operand2);
    } else {
        throw error_t(
            format("representation should contain 3 elements for operator %s", filter_operator));
    }
}
}
}  // namesapce cocaine::logging
