#pragma once

#include <algorithm>
#include <exception>
#include <memory>
#include <vector>

#include <blackhole/logger.hpp>
#include <blackhole/scope/holder.hpp>
#include <blackhole/wrapper.hpp>

#include <cocaine/auth/uid.hpp>
#include <cocaine/format.hpp>
#include <cocaine/logging.hpp>
#include <cocaine/locked_ptr.hpp>

#include <boost/algorithm/string/join.hpp>
#include <boost/range/adaptor/sliced.hpp>

#include "backend/fabric.hpp"

namespace cocaine { namespace unicat {

namespace detail {
    constexpr auto TAKE_EXCEPT_TRACE = size_t{10};
}

struct completion_t final {
    enum Opcode { Nop, ReadOp, WriteOp };

    url_t url;
    std::shared_ptr<auth::identity_t> identity;

    bool done;

    // TODO: ugly error representation, refactor someday.
    std::error_code err_code;
    Opcode opcode;
    std::exception_ptr eptr;

    explicit completion_t(const url_t, std::shared_ptr<auth::identity_t>&);
    completion_t(const url_t, std::shared_ptr<auth::identity_t>&, const std::error_code ec, const Opcode);
    completion_t(const url_t, std::shared_ptr<auth::identity_t>&, std::exception_ptr);

    auto make_access_error() const -> std::string;
    auto make_exception_error() const -> std::string;

    auto has_exception() const -> bool;
    auto has_error_code() const -> bool;
};

auto opcode_to_string(completion_t::Opcode) -> std::string;

struct base_completion_state_t {
    virtual ~base_completion_state_t() = default;
    virtual auto set_completion(completion_t completion) -> void = 0;
};

template<typename Upstream, typename Protocol>
struct async_completion_state_t : public base_completion_state_t {
    using compl_state_type = std::vector<completion_t>;
    synchronized<compl_state_type> state;

    Upstream upstream;
    std::unique_ptr<logging::logger_t> log;
    const std::string operation_name;

    explicit async_completion_state_t(Upstream upstream, std::unique_ptr<logging::logger_t> log,
        const std::string& operation_name) :
        upstream(std::move(upstream)),
        log(std::move(log)),
        operation_name(operation_name)
    {}

    virtual ~async_completion_state_t() {
        auto count = state.synchronize()->size();

        COCAINE_LOG_INFO(log,
            "async {} operation done for {} handlers", operation_name, count);
        finalize();
    }

    auto set_completion(completion_t completion) -> void override {
        state.apply([=] (compl_state_type& state) {
            state.push_back(std::move(completion));
        });
    }

    auto finalize() -> void {
        using namespace boost::adaptors;

        auto errors_list = state.apply([&] (compl_state_type& state) {
            // TODO: make errors with <code, string> pairs
            std::vector<std::string> errors;

            for(const auto& comp : state) {
                if (comp.has_exception()) {
                    const auto err_message = comp.make_exception_error();

                    COCAINE_LOG_WARNING(log, "access operation '{}' failed due access error for {}:{}:{} with cids {} uids {}, exception: {}",
                        operation_name,
                        scheme_to_string(comp.url.scheme), comp.url.service_name, comp.url.entity,
                        comp.identity->cids(), comp.identity->uids(),
                        err_message
                    );

                    errors.push_back(comp.make_exception_error());
                } else if (comp.has_error_code()) {
                    const auto err_message = comp.make_access_error();

                    COCAINE_LOG_WARNING(log, "{} part of {} operation failed due exception '{}' for {}:{}:{} with cids {} uids {}",
                        opcode_to_string(comp.opcode),
                        operation_name,
                        err_message,
                        scheme_to_string(comp.url.scheme), comp.url.service_name, comp.url.entity,
                        comp.identity->cids(), comp.identity->uids()
                    );

                    errors.push_back(err_message);
                } else {
                    COCAINE_LOG_INFO(log, "operation {} completed for {}:{}:{} with cids {} uids {}",
                        operation_name,
                        scheme_to_string(comp.url.scheme), comp.url.service_name, comp.url.entity,
                        comp.identity->cids(), comp.identity->uids());
                }
            }

            return errors;
        });

        try {
            const auto count = state.synchronize()->size();

            if (errors_list.empty()) {
                COCAINE_LOG_INFO(log,
                    "all {} completions have been done with success, report result to client", count);
                upstream.template send<typename Protocol::value>();
                return;
            }

            const auto to_take = std::min(errors_list.size(), detail::TAKE_EXCEPT_TRACE);
            COCAINE_LOG_WARNING(log,
                "there ware {} exception(s), reporting first {} to client", errors_list.size(), to_take);

            upstream.template send<typename Protocol::error>(
                error::uncaught_error,
                boost::join(errors_list | sliced(0, to_take), ", "));
        } catch(const std::system_error& e) {
            COCAINE_LOG_WARNING(log,
                "failed to send completion result with {} {}", e.code(), e.what());
        } catch(const std::exception& e) {
            COCAINE_LOG_WARNING(log, "failed to send completion result with unknown exception: {}", e.what());
        } // try/catch
    }
};

}
}
