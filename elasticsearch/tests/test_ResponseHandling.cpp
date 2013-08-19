#include <tuple>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "../src/rest/get.hpp"
#include "../src/rest/index.hpp"
#include "../src/rest/search.hpp"

using namespace cocaine::service;

using namespace ::testing;

template<typename T>
class deferred_mock {
public:
    MOCK_METHOD1(write, void(const std::tuple<bool, std::string>&));
    MOCK_METHOD2(abort, void(int, const std::string&));
};

TEST(get_handler_t, NormalResponse) {
    get_handler_t handler;
    deferred_mock<response::get> deferred;
    const std::string response = "{answer}";
    EXPECT_CALL(deferred, write(std::make_tuple(true, std::string("{answer}"))))
            .Times(1);
    handler(deferred, 200, response);
}

TEST(get_handler_t, NotFoundResponseWithMissingIndex) {
    get_handler_t handler;
    deferred_mock<response::get> deferred;
    const std::string response = R"({"error":"IndexMissingException[[index] missing]","status":404})";
    EXPECT_CALL(deferred, write(std::make_tuple(false, std::string("IndexMissingException[[index] missing][404]"))))
            .Times(1);
    handler(deferred, 404, response);
}

TEST(get_handler_t, NotFoundResponse) {
    get_handler_t handler;
    deferred_mock<response::get> deferred;
    const std::string response = R"({"_index":"twitter","_type":"tweet","_id":"0","exists":false})";
    EXPECT_CALL(deferred, write(std::make_tuple(false, std::string("[404]"))))
            .Times(1);
    handler(deferred, 404, response);
}

TEST(get_handler_t, AbortOnErrorAndNotValidJsonReceived) {
    get_handler_t handler;
    deferred_mock<response::get> deferred;
    const std::string response = "Not a json";
    EXPECT_CALL(deferred, abort(-1, "parsing failed - Expect either an object or array at root"))
            .Times(1);
    handler(deferred, 400, response);
}

TEST(index_handler_t, NormalUsage) {
    index_handler_t handler;
    deferred_mock<response::search> deferred;
    const std::string response = R"({"ok":true,"_index":"twitter","_type":"tweet","_id":"1","_version":10})";
    EXPECT_CALL(deferred, write(std::make_tuple(true, std::string("1"))))
            .Times(1);
    handler(deferred, 200, response);
}

TEST(index_handler_t, AbortOnParsingFailed) {
    index_handler_t handler;
    deferred_mock<response::search> deferred;
    const std::string response = "not valid json";
    EXPECT_CALL(deferred, abort(-1, "parsing failed - Expect either an object or array at root"))
            .Times(1);
    handler(deferred, 200, response);
}

TEST(index_handler_t, MessageWhenIndexIdNotReturned) {
    index_handler_t handler;
    deferred_mock<response::search> deferred;
    const std::string response = R"({"ok":false,"_index":"twitter","_type":"tweet"})";
    EXPECT_CALL(deferred, write(std::make_tuple(false, std::string("error - response has no field '_id'"))))
            .Times(1);
    handler(deferred, 200, response);
}
