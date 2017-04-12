/*
* 2015+ Copyright (c) Anton Matveenko <antmat@yandex-team.ru>
* All rights reserved.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*/

#include <memory>

#include "cocaine/idl/isolate.hpp"

#include <cocaine/dynamic.hpp>
#include <cocaine/context.hpp>

#include <blackhole/logger.hpp>

#include <cocaine/api/service.hpp>
#include <cocaine/logging.hpp>
#include <cocaine/repository.hpp>
#include <cocaine/repository/service.hpp>
#include <cocaine/rpc/dispatch.hpp>
#include <cocaine/traits/dynamic.hpp>

#include <sys/time.h>
#include <sys/resource.h>

namespace cocaine { namespace service {

namespace aux {
    struct formatter_t {
        auto
        operator()(const std::map<std::string, std::string>& data) -> std::string {
            std::string result;
            for(auto& pair: data) {
                if(!result.empty()) {
                    result += ", ";
                }
                result += pair.first + ":" + pair.second;
            }
            return result;
        }
    };
}

std::unique_ptr<cocaine::logging::logger_t> log;

struct spooled_dispatch_t: public dispatch<io::isolate_spooled_tag> {
    spooled_dispatch_t() :
        dispatch<io::isolate_spooled_tag>("spooled_isolate")
    {
        on<io::isolate_spooled::cancel>([](){
            COCAINE_LOG_INFO(log, "spool::cancel");
        });
    }

};

struct spawned_dispatch_t: public dispatch<io::isolate_spawned_tag> {
    spawned_dispatch_t() :
        dispatch<io::isolate_spawned_tag>("spawned_isolate")
    {
        on<io::isolate_spawned::kill>([](){
            COCAINE_LOG_INFO(log, "spawn::kill");
        });
    }

};

struct spool_slot_t:
    public io::basic_slot<io::isolate::spool>
{
public:
    typedef typename io::aux::protocol_impl<typename io::event_traits<io::isolate::spool>::upstream_type>::type protocol;

    virtual
    result_type
    operator()(const std::vector<hpack::header_t>&, tuple_type&& args, upstream_type&& upstream)
    {
        COCAINE_LOG_INFO(log, "spool. {}, {}", boost::lexical_cast<std::string>(std::get<0>(args)), std::get<1>(args));
        sleep(1);
        upstream.send<protocol::value>();
        COCAINE_LOG_INFO(log, "spool. sent value");
        return result_type(std::make_shared<spooled_dispatch_t>());
    }
};

struct spawn_slot_t:
    public io::basic_slot<io::isolate::spawn>
{
public:
    typedef typename io::aux::protocol_impl<typename io::event_traits<io::isolate::spawn>::upstream_type>::type protocol;

    virtual
    result_type
    operator()(tuple_type&& args, upstream_type&& upstream)
    {
        return operator()({}, std::move(args), std::move(upstream));
    }

    virtual
    result_type    
    operator()(const std::vector<hpack::header_t>&, tuple_type&& args, upstream_type&& upstream)
    {
        aux::formatter_t formatter;

        COCAINE_LOG_INFO(log, "spawn. {}, {}, {}, {}, {}",
                         boost::lexical_cast<std::string>(std::get<0>(args)),
                         std::get<1>(args),
                         std::get<2>(args),
                         formatter(std::get<3>(args)),
                         formatter(std::get<4>(args))
        );
        sleep(1);
        upstream = upstream.send<protocol::chunk>("");
        sleep(1);
        upstream =  upstream.send<protocol::chunk>("CHUNK1");
        sleep(1);
        upstream = upstream.send<protocol::chunk>("CHUNK2");
        sleep(1);
        upstream = upstream.send<protocol::chunk>("CHUNK3");
        sleep(1);
        upstream.send<protocol::choke>();
        COCAINE_LOG_INFO(log, "spawn. done");
        return result_type(std::make_shared<spawned_dispatch_t>());
    }
};

struct metrics_slot_t:
    public io::basic_slot<io::isolate::metrics>
{
public:
    typedef typename io::aux::protocol_impl<typename io::event_traits<io::isolate::metrics>::upstream_type>::type protocol;

    virtual
    result_type
    operator()(tuple_type&& args, upstream_type&& upstream)
    {
        return operator()({}, std::move(args), std::move(upstream));
    }

    virtual
    result_type
    operator()(const std::vector<hpack::header_t>&, tuple_type&& args, upstream_type&& upstream)
    {
        const auto& query = std::get<0>(args);
        COCAINE_LOG_INFO(log, "metrics. {}", boost::lexical_cast<std::string>(query));

        sleep(1);

        rusage ru;
        // note: values used as dummies and don't match real metrics meanings
        if (getrusage(RUSAGE_SELF, &ru)) {
            // TODO: send errors?
        }

        dynamic_t::array_t metrics_array;
        dynamic_t::object_t uuids;

        for(const auto& uuid : query.as_object()["uuids"].as_array()) {
                dynamic_t::object_t common;
                dynamic_t::object_t metrics;

                const auto total_time = ru.ru_utime.tv_sec + ru.ru_stime.tv_sec;
                const auto cpu = total_time ?
                    ru.ru_utime.tv_sec / total_time * 100 :
                    0;

                metrics["cpu.gauge"] = static_cast<std::uint64_t>(cpu % 100);

                metrics["user_time.gauge"] = ru.ru_utime.tv_sec;
                metrics["sys_time.gauge"] =  ru.ru_stime.tv_sec;

                metrics["rss.gauge"] = ru.ru_maxrss;

                metrics["ioread.counter"]  = ru.ru_inblock;
                metrics["iowrite.counter"] = ru.ru_oublock;

                common["common"] = metrics;
                uuids[uuid.as_string()] = common;
        }

        dynamic_t::object_t answer;
        answer["test_app"] = uuids;

        upstream.send<protocol::value>(answer);

        COCAINE_LOG_INFO(log, "metrics. done");

        return boost::none;
    }
};

class isolate_mock_t:
public api::service_t,
public dispatch<io::isolate_tag>
{
public:
    isolate_mock_t(context_t& context, asio::io_service& asio, const std::string& name, const dynamic_t& args) :
        api::service_t(context, asio, name, args),
        dispatch<io::isolate_tag>(name)
    {
        log = context.log("isolate_mock");
        on<io::isolate::spool>(std::make_shared<spool_slot_t>());
        on<io::isolate::spawn>(std::make_shared<spawn_slot_t>());
        on<io::isolate::metrics>(std::make_shared<metrics_slot_t>());
    }

    virtual
    auto
    prototype() -> io::basic_dispatch_t& {
        return *this;
    }

private:
};
}}

extern "C" {
auto
validation() -> cocaine::api::preconditions_t {
    return cocaine::api::preconditions_t { COCAINE_MAKE_VERSION(0, 12, 5) };
}

void
initialize(cocaine::api::repository_t& repository) {
    repository.insert<cocaine::service::isolate_mock_t>("isolate_mock");
}
}
