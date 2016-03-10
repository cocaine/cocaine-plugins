#include <asio/io_service.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

// node -> [app]
// app -> state
// state -> thread + loop. on stop -> work.reset + timer(loop.stop) +
// thread.join.
// state -> overseer.
// overseer -> [slave]
// slave -> state_machine
// state_machine -> overseer.
// all secondary operations must be performed in a loop thread.

namespace cocaine {
namespace detail {
namespace service {
namespace node {

namespace io {

using asio::io_service;

}  // namespace io

template <class Overseer>
class state_machine {
public:
    typedef Overseer overseer_type;

private:
    std::shared_ptr<overseer_type> overseer;

    std::error_code shutdown_reason;

public:
    explicit state_machine(std::shared_ptr<overseer_type> overseer)
        : overseer(std::move(overseer)) {}

    ~state_machine() {
        const auto uuid = "";
        const auto on_death =
            std::bind(&overseer_type::notify_death, overseer, uuid, shutdown_reason);

        overseer->get_io_service().post(std::move(on_death));
    }
};

template <class Manifest, class Profile, class Overseer>
class slave {
public:
    typedef Manifest manifest_type;
    typedef Profile profile_type;
    typedef Overseer overseer_type;

private:
    typedef state_machine<Overseer> machine_t;

private:
    std::shared_ptr<machine_t> d;

public:
    /// Constructs the slave.
    slave(manifest_type manifest, profile_type profile, std::shared_ptr<overseer_type> overseer)
        : d(std::make_shared<machine_t>(std::move(overseer))) {}

    ~slave() {}
};

}  // namespace node
}  // namespace service
}  // namespace detail
}  // namespace cocaine

namespace io {

using asio::io_service;

}  // namespace io

namespace testing {
namespace mock {

struct manifest_t {};
struct profile_t {};

struct overseer_t {
    MOCK_METHOD0(get_io_service, io::io_service&());

    MOCK_METHOD2(notify_death, void(const std::string&, const std::error_code&));
};

}  // namespace mock
}  // namespace testing

namespace testing {

typedef cocaine::detail::service::node::slave<mock::manifest_t, mock::profile_t, mock::overseer_t>
    slave_t;

TEST(slave, constructor) {
    mock::manifest_t manifest;
    mock::profile_t profile;
    auto overseer = std::make_shared<mock::overseer_t>();

    io::io_service io;
    EXPECT_CALL(*overseer, get_io_service()).Times(1).WillOnce(ReturnRef(io));

    slave_t slave(std::move(manifest), std::move(profile), overseer);
}

TEST(slave, notifies_on_death) {
    mock::manifest_t manifest;
    mock::profile_t profile;
    auto overseer = std::make_shared<mock::overseer_t>();

    io::io_service io;

    std::unique_ptr<slave_t> slave(new slave_t(std::move(manifest), std::move(profile), overseer));

    EXPECT_CALL(*overseer, get_io_service()).Times(1).WillOnce(ReturnRef(io));
    EXPECT_CALL(*overseer, notify_death(_, std::error_code())).Times(1);

    // Slave will post its completion handler during state machine destruction.
    slave.reset();

    io.run();
}

}  // namespace testing
