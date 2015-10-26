
#ifndef COCAINE_CONDUCTOR_COMMON_HPP
#define COCAINE_CONDUCTOR_COMMON_HPP

#include <cocaine/context.hpp>
#include <cocaine/api/isolate.hpp>
#include <cocaine/api/service.hpp>
#include <cocaine/rpc/dispatch.hpp>

#include <cocaine/logging.hpp>
#include <cocaine/memory.hpp>

#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <system_error>
#include <atomic>

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/lexical_cast.hpp>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "cocaine/idl/conductor.hpp"

namespace cocaine { namespace isolate { namespace conductor {

namespace ph = std::placeholders;

using std::enable_shared_from_this;
using std::make_shared;
using std::shared_ptr;
using std::make_unique;
using std::unique_ptr;



struct handle_t;
class isolate_t;
class client_t;
struct container_t;

namespace state {


using std::enable_shared_from_this;
using std::make_shared;
using std::shared_ptr;
using std::make_unique;
using std::unique_ptr;

class base_t;
class closed_t;
class connecting_t;
class connected_t;

}

namespace action {

class action_t;

class spool_t;
class spawn_t;
class terminate_t;

struct cancellation_t;

}


}}}

#endif

