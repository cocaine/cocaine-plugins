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

#include "cocaine/idl/logging_v2.hpp"

#include "cocaine/logging/attribute.hpp"

#include <cocaine/forwards.hpp>
#include <cocaine/logging.hpp>

#include <cocaine/api/service.hpp>

#include <cocaine/rpc/dispatch.hpp>
#include <cocaine/rpc/slot/deferred.hpp>

namespace cocaine {
namespace logging {
class metafilter_t;
}
}

namespace cocaine {
namespace service {

class logging_v2_t : public api::service_t, public dispatch<io::base_log_tag> {
public:
    virtual const io::basic_dispatch_t& prototype() const;

    logging_v2_t(context_t& context, asio::io_service& asio, const std::string& name, const dynamic_t& args);

    struct impl_t;

private:
    class logging_slot_t;

    std::shared_ptr<impl_t> d;
};

class named_logging_t : public dispatch<io::named_log_tag> {
public:
    named_logging_t(logging::logger_t& log, std::string name, std::shared_ptr<logging::metafilter_t> filter);

private:
    logging::logger_t& log;
    std::string backend;
    std::shared_ptr<logging::metafilter_t> filter;
};
}
}  // namespace cocaine::service
