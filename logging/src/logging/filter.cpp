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
#include <cocaine/trace/trace.hpp>

#include <blackhole/attribute.hpp>

#include <boost/optional/optional.hpp>

namespace cocaine {
namespace logging {

namespace {

auto operator<(const blackhole::stdext::string_view& lhs, const std::string& rhs) -> bool
{
    return std::lexicographical_compare(lhs.data(), lhs.data() + lhs.size(),
                                        rhs.data(), rhs.data() + rhs.size());
}

auto operator>(const blackhole::stdext::string_view& lhs, const std::string& rhs) -> bool
{
    std::greater<char> comp;
    return std::lexicographical_compare(lhs.data(), lhs.data() + lhs.size(),
                                        rhs.data(), rhs.data() + rhs.size(), comp);
}

auto operator<=(const blackhole::stdext::string_view& lhs, const std::string& rhs) -> bool
{
    return !(lhs > rhs);
}

auto operator>=(const blackhole::stdext::string_view& lhs, const std::string& rhs) -> bool
{
    return !(lhs < rhs);
}

template <class T>
struct view_of {
    typedef T type;
};

template <>
struct view_of<std::string> {
    typedef blackhole::stdext::string_view type;
};


template <class T>
struct to;

template<>
struct to<int64_t> : public blackhole::attribute::view_t::visitor_t {
    typedef blackhole::attribute::view_t value_t;
    static constexpr int64_t max = std::numeric_limits<int64_t>::max();

    int64_t result;
    bool failed;

    to() : result(), failed() {}

    void fail() {
        failed = true;
    }

    virtual auto operator()(const value_t::null_type&) -> void {
        result = 0;
    }

    virtual auto operator()(const value_t::bool_type& val) -> void {
        result = static_cast<int64_t>(val);
    }

    virtual auto operator()(const value_t::sint64_type& val) -> void {
        result = val;
    }
    virtual auto operator()(const value_t::uint64_type& val) -> void {
        if(val > max) {
            fail();
        }
        result = val;
    }
    virtual auto operator()(const value_t::double_type& val) -> void {
        result = static_cast<int64_t>(val);
    }
    virtual auto operator()(const value_t::string_type&) -> void {
        fail();
    }
    virtual auto operator()(const value_t::function_type&) -> void {
        fail();
    }
};

template<>
struct to<blackhole::stdext::string_view> : public blackhole::attribute::view_t::visitor_t {
    typedef blackhole::attribute::view_t value_t;

    blackhole::stdext::string_view result;
    bool failed;

    to() : result(), failed() {}

    void fail() {
        failed = true;
    }

    virtual auto operator()(const value_t::null_type&) -> void {
    }

    virtual auto operator()(const value_t::bool_type&) -> void {
        fail();
    }

    virtual auto operator()(const value_t::sint64_type&) -> void {
        fail();
    }
    virtual auto operator()(const value_t::uint64_type&) -> void {
        fail();
    }
    virtual auto operator()(const value_t::double_type&) -> void {
        fail();
    }
    virtual auto operator()(const value_t::string_type& val) -> void {
        result = val;
    }
    virtual auto operator()(const value_t::function_type&) -> void {
        fail();
    }
};

template<>
struct to<double> : public blackhole::attribute::view_t::visitor_t {
    typedef blackhole::attribute::view_t value_t;

    double result;
    bool failed;

    to() : result(), failed() {}

    void fail() {
        failed = true;
    }

    virtual auto operator()(const value_t::null_type&) -> void {
    }

    virtual auto operator()(const value_t::bool_type& val) -> void {
        result = static_cast<double>(val);
    }

    virtual auto operator()(const value_t::sint64_type& val) -> void {
        result = val;
    }
    virtual auto operator()(const value_t::uint64_type& val) -> void {
        result = val;
    }
    virtual auto operator()(const value_t::double_type& val) -> void {
        result = val;
    }
    virtual auto operator()(const value_t::string_type&) -> void {
        fail();
    }
    virtual auto operator()(const value_t::function_type&) -> void {
        fail();
    }
};

template<class T>
typename std::enable_if<std::is_integral<T>::value, bool>::type
convert(const blackhole::attribute::view_t& value, T& target) {
    to<int64_t> visitor;
    value.apply(visitor);
    if(!visitor.failed) {
        target = static_cast<T>(visitor.result);
        return true;
    }
    return false;
}

template<class T>
typename std::enable_if<std::is_floating_point<T>::value, bool>::type
convert(const blackhole::attribute::view_t& value, T& target) {
    to<double> visitor;
    value.apply(visitor);
    if(!visitor.failed) {
        target = static_cast<T>(visitor.result);
        return true;
    }
    return false;
}

template<class T>
bool
convert(const blackhole::attribute::view_t& value, blackhole::stdext::string_view& target) {
    to<blackhole::stdext::string_view> visitor;
    value.apply(visitor);
    if(!visitor.failed) {
        target = visitor.result;
        return true;
    }
    return false;
}

inline boost::optional<const attribute_view_t&> find_attribute(const blackhole::attribute_pack& attribute_pack,
                                                          const std::string& attribute_name) {
    for(const auto& attributes : attribute_pack) {
        for(const auto& attribute : attributes.get()) {
            if(attribute.first == attribute_name) {
                return boost::optional<const attribute_view_t&>(attribute);
            }
        }
    }
    return boost::none;
}

}

filter_info_t::filter_info_t(filter_t _filter,
                             filter_t::deadline_t _deadline,
                             filter_t::id_t _id,
                             filter_t::disposition_t _disposition,
                             std::string _logger_name)
    : filter(std::move(_filter)),
      deadline(std::move(_deadline)),
      id(_id),
      disposition(_disposition),
      logger_name(std::move(_logger_name)) {}

filter_info_t::filter_info_t(const dynamic_t& value) {
    const auto throw_error = [&] {
        throw cocaine::error_t("invalid representation for filter info - {}", boost::lexical_cast<std::string>(value));
    };
    if (!value.is_object()) {
        throw_error();
    }
    const auto& obj = value.as_object();
    static const std::vector<std::string> required_keys {"filter", "deadline", "id", "logger_name", "disposition"};
    for(const auto& key: required_keys) {
        if(!obj.count(key)) {
            throw_error();
        }
    }
    filter = filter_t(obj.at("filter"));
    typedef filter_t::deadline_t dl_t;
    deadline = dl_t(dl_t::duration(obj.at("deadline").as_uint()));
    id = obj.at("id").as_uint();

    disposition = static_cast<filter_t::disposition_t>(obj.at("disposition").as_uint());
    if(disposition != filter_t::disposition_t::cluster && disposition != filter_t::disposition_t::local) {
        throw_error();
    }

    logger_name = obj.at("logger_name").as_string();
}

dynamic_t filter_info_t::representation() {
    dynamic_t::object_t container;
    container["filter"] = filter.representation();
    container["deadline"] = deadline.time_since_epoch().count();
    container["id"] = id;
    container["logger_name"] = logger_name;
    container["disposition"] = static_cast<uint64_t>(disposition);
    return dynamic_t(std::move(container));
}

class filter_t::inner_t {
public:
    virtual ~inner_t() {}

    virtual filter_result_t apply(blackhole::severity_t severity,
                                  blackhole::attribute_pack& attributes) const = 0;

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
    virtual filter_result_t apply(blackhole::severity_t,
                                  blackhole::attribute_pack&) const {
        throw std::system_error(std::make_error_code(std::errc::operation_not_supported),
                                "tried to apply null filter");
    }

    virtual dynamic_t representation() const {
        throw std::system_error(std::make_error_code(std::errc::operation_not_supported),
                                "tried to get representation of null filter");
    };
};

struct empty_filter_t : public filter_t::inner_t {
    virtual filter_result_t apply(blackhole::severity_t,
                                  blackhole::attribute_pack&) const {
        return fr::accept;
    }

    virtual dynamic_t representation() const {
        return dynamic_t(std::make_tuple(std::string("empty")));
    };
};

struct severity_filter_t : public filter_t::inner_t {
    blackhole::severity_t ref_severity;

    severity_filter_t(blackhole::severity_t severity) : ref_severity(severity) {}

    virtual filter_result_t apply(blackhole::severity_t severity,
                                  blackhole::attribute_pack&) const {
        return severity >= ref_severity ? fr::accept : fr::reject;
    }

    virtual dynamic_t representation() const {
        return dynamic_t(std::make_tuple(std::string("severity"), static_cast<int>(ref_severity)));
    };
};

struct exists_filter_t : public filter_t::inner_t {
    exists_filter_t(std::string _attribute_name) : attribute_name(std::move(_attribute_name)) {}

    virtual filter_result_t apply(blackhole::severity_t,
                                  blackhole::attribute_pack& attributes) const {
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

    virtual filter_result_t apply(blackhole::severity_t,
                                  blackhole::attribute_pack& attributes) const {
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

    virtual filter_result_t apply(blackhole::severity_t,
                                  blackhole::attribute_pack& attributes) const {
        const auto& val = logging::find_attribute(attributes, this->attribute_name);
        if(!val) {
            return fr::reject;
        }
        typedef typename view_of<T>::type view_t;
        view_t result;
        bool convertible = convert<view_t>(val->second, result);
        return (convertible && result == this->attribute_value) ? fr::accept : fr::reject;
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

    virtual filter_result_t apply(blackhole::severity_t,
                                  blackhole::attribute_pack& attributes) const {
        const auto& val = logging::find_attribute(attributes, this->attribute_name);
        if(!val) {
            return fr::accept;
        }
        typedef typename view_of<T>::type view_t;
        view_t result;
        bool convertible = convert<view_t>(val->second, result);
        return (convertible && result == this->attribute_value) ? fr::reject : fr::accept;
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

    virtual filter_result_t apply(blackhole::severity_t,
                                  blackhole::attribute_pack& attributes) const {
        const auto& val = logging::find_attribute(attributes, this->attribute_name);
        if(!val) {
            return fr::reject;
        }
        typedef typename view_of<T>::type view_t;
        view_t result;
        bool convertible = convert<view_t>(val->second, result);
        return (convertible && result > this->attribute_value) ? fr::accept : fr::reject;
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

    virtual filter_result_t apply(blackhole::severity_t,
                                  blackhole::attribute_pack& attributes) const {
        const auto& val = logging::find_attribute(attributes, this->attribute_name);
        if(!val) {
            return fr::reject;
        }
        typedef typename view_of<T>::type view_t;
        view_t result;
        bool convertible = convert<view_t>(val->second, result);
        return (convertible && result < this->attribute_value) ? fr::accept : fr::reject;
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

    virtual filter_result_t apply(blackhole::severity_t,
                                  blackhole::attribute_pack& attributes) const {
        const auto& val = logging::find_attribute(attributes, this->attribute_name);
        if(!val) {
            return fr::reject;
        }
        typedef typename view_of<T>::type view_t;
        view_t result;
        bool convertible = convert<view_t>(val->second, result);
        return (convertible && result >= this->attribute_value) ? fr::accept : fr::reject;
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

    virtual filter_result_t apply(blackhole::severity_t,
                                  blackhole::attribute_pack& attributes) const {
        const auto& val = logging::find_attribute(attributes, this->attribute_name);
        if(!val) {
            return fr::reject;
        }
        typedef typename view_of<T>::type view_t;
        view_t result;
        bool convertible = convert<view_t>(val->second, result);
        return (convertible && result <= this->attribute_value) ? fr::accept : fr::reject;
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

    virtual filter_result_t apply(blackhole::severity_t severity,
                                  blackhole::attribute_pack& attributes) const {
        return (filter1.apply(severity, attributes) == fr::accept ||
                filter2.apply(severity, attributes) == fr::accept)
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

    virtual filter_result_t apply(blackhole::severity_t severity,
                                  blackhole::attribute_pack& attributes) const {
        return (filter1.apply(severity, attributes) == fr::accept &&
                filter2.apply(severity, attributes) == fr::accept)
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

    virtual filter_result_t apply(blackhole::severity_t severity,
                                  blackhole::attribute_pack& attributes) const {
        return filter1.apply(severity, attributes) !=
                       filter2.apply(severity, attributes)
                   ? fr::accept
                   : fr::reject;
    }

    virtual dynamic_t representation() const {
        return dynamic_t(std::make_tuple(
            std::string("xor"), filter1.representation(), filter2.representation()));
    };
};

struct traced_filter_t : public filter_t::inner_t {
    traced_filter_t() {}

    virtual filter_result_t apply(blackhole::severity_t, blackhole::attribute_pack&) const {
        return trace_t::current().verbose() ? fr::accept : fr::reject;
    }

    virtual dynamic_t representation() const {
        return dynamic_t(std::make_tuple(std::string("traced")));
    };
};

inner_t unary_operator_factory(const std::string& filter_operator, const dynamic_t& operand) {
    if (operand.is_string()) {
        if (filter_operator == "e") {
            return inner_t(new exists_filter_t(operand.as_string()));
        } else if (filter_operator == "!e") {
            return inner_t(new not_exists_filter_t(operand.as_string()));
        }
    } else if (operand.is_uint() && filter_operator == "severity") {
        return inner_t(new severity_filter_t(operand.as_uint()));
    }
    throw error_t(format("invalid unary filter operator: {}", filter_operator));
}

inner_t string_operand_factory(const std::string& filter_operator,
                               const dynamic_t& operand1,
                               const dynamic_t& operand2) {
    if (!operand1.is_string()) {
        throw error_t(format("operand 1 for operator {} should be strings", filter_operator));
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
    throw std::logic_error(format("Invalid operator passed: {}", filter_operator));
}

inner_t filter_operand_factory(const std::string& filter_operator,
                               const dynamic_t& operand1,
                               const dynamic_t& operand2) {
    if (!operand1.is_array() || !operand2.is_array()) {
        throw error_t(format("operands for operator {} should be arrays", filter_operator));
    } else if (filter_operator == "||") {
        return inner_t(new or_filter_t(filter_t(operand1), filter_t(operand2)));
    } else if (filter_operator == "&&") {
        return inner_t(new and_filter_t(filter_t(operand1), filter_t(operand2)));
    } else if (filter_operator == "xor") {
        return inner_t(new xor_filter_t(filter_t(operand1), filter_t(operand2)));
    }
    throw std::logic_error(format("Invalid operator passed: {}", filter_operator));
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

filter_result_t filter_t::apply(blackhole::severity_t severity,
                                blackhole::attribute_pack& attributes) const {
    return inner->apply(severity, attributes);
}

dynamic_t filter_t::representation() const {
    return inner->representation();
}

filter_t::filter_t(const dynamic_t& source) {
    if (!source.is_array()) {
        throw error_t("representation should be array, found - {}",
                      boost::lexical_cast<std::string>(source));
    }
    const auto& array = source.as_array();
    if (array.size() < 1) {
        throw error_t("representation should contain  at least 1 element");
    }
    if (!array[0].is_string()) {
        throw error_t("operator should be string");
    }
    const auto& filter_operator = array[0].as_string();
    const auto& operand1 = array[1];
    if (array.size() == 1 && array[0].is_string()) {
        if(array[0].as_string() == "empty") {
            inner.reset(new empty_filter_t());
        } else if(array[0].as_string() == "traced") {
            inner.reset(new traced_filter_t());
        } else {
            throw error_t("unknown single argument filter specification", array[0].as_string());
        }
    } else if (array.size() == 2) {
        inner = unary_operator_factory(filter_operator, operand1);
    } else if (array.size() == 3) {
        const auto operand2 = array[2];
        inner = binary_operator_factory(filter_operator, operand1, operand2);
    } else {
        throw error_t("representation should contain 3 elements for operator {}", filter_operator);
    }
}
}
}  // namesapce cocaine::logging
