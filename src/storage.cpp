/*
 * Copyright 2013+ Ruslan Nigmatullin <euroelessar@yandex.ru>
 * Copyright 2011-2012 Andrey Sibiryov <me@kobology.ru>
 *
 * This file is part of Elliptics.
 *
 * Elliptics is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Elliptics is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with Elliptics.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <cocaine/storage.hpp>

#include <cocaine/context.hpp>
#include <cocaine/dynamic.hpp>
#include <cocaine/logging.hpp>

#include <blackhole/formatter/string.hpp>
#include <blackhole/v1/attribute.hpp>
#include <blackhole/v1/logger.hpp>

namespace cocaine { namespace storage {

namespace ph = std::placeholders;

using namespace cocaine;
using namespace cocaine::logging;
using namespace cocaine::storage;
namespace ell = ioremap::elliptics;

static logging::priorities convert(ell::log_level level) {
	switch (level) {
	case DNET_LOG_DEBUG:
		return logging::debug;
	case DNET_LOG_NOTICE:
	case DNET_LOG_INFO:
		return logging::info;
	case DNET_LOG_WARNING:
		return logging::warning;
	case DNET_LOG_ERROR:
	default:
		return logging::error;
	};
}

class frontend_t : public blackhole::base_frontend_t {
public:
	frontend_t(std::shared_ptr<logging::logger_t> log, ell::log_level severity)
	: log(std::move(log))
	, severity(severity)
	, formatter("%(message)s %(...::)s") {}

	virtual void handle(const blackhole::log::record_t& record) {
		const auto level =
		    record.extract<dnet_log_level>(blackhole::keyword::severity<dnet_log_level>().name());

		if (level < severity) {
			return;
		}

		const auto mapped = convert(level);

		log->log(static_cast<int>(mapped), formatter.format(record));
	}

private:
	std::shared_ptr<logging::logger_t> log;
	ell::log_level severity;
	blackhole::formatter::string_t formatter;
};

log_adapter_t::log_adapter_t(std::shared_ptr<logging::logger_t> log, ell::log_level level)
: ell::logger_base() {
	verbosity(DNET_LOG_DEBUG);
	add_frontend(std::unique_ptr<frontend_t>(new frontend_t(log, level)));
}

namespace {

dnet_config parse_json_config(const dynamic_t::object_t& args) {
	dnet_config cfg;

	std::memset(&cfg, 0, sizeof(cfg));

	cfg.wait_timeout   = args.at("wait-timeout", 5u).as_uint();
	cfg.check_timeout  = args.at("check-timeout", 20u).as_uint();
	cfg.io_thread_num  = args.at("io-thread-num", 0u).as_uint();
	cfg.net_thread_num = args.at("net-thread-num", 0u).as_uint();
	cfg.flags          = args.at("flags", 0u).as_uint();
	return cfg;
}

}

elliptics_storage_t::elliptics_storage_t(context_t &context, const std::string &name, const dynamic_t &args)
: category_type(context, name, args)
, m_context(context)
, m_log(context.log(name)) // TODO: It was with attributes: {{"storage", "elliptics"}}.
, // XXX: dynamic_t from cocaine can't convert int to uint, and DNET_LOG_INFO being an enum value is int
  m_log_adapter(m_log, static_cast<ioremap::elliptics::log_level>(
                           args.as_object().at("verbosity", uint(DNET_LOG_INFO)).as_uint()))
, m_read_latest(args.as_object().at("read_latest", false).as_bool())
, m_config(parse_json_config(args.as_object()))
, m_node(ell::logger(m_log_adapter, blackhole::log::attributes_t{{"storage", {"elliptics"}}}), m_config)
, m_session(m_node) {
	dynamic_t::array_t nodes = args.as_object().at("nodes").as_array();

	if (nodes.empty()) {
		throw std::system_error(std::make_error_code(std::errc::invalid_argument),
		                        "no nodes has been specified");
	}

	std::vector<ioremap::elliptics::address> remotes;
	for (auto it = nodes.begin(); it != nodes.end(); ++it) {
		try {
			remotes.emplace_back((*it).as_string());
		} catch (ioremap::elliptics::error &exc) {
			const std::string fmt("failed to parse remote: ");
			throw std::system_error(std::make_error_code(std::errc::invalid_argument),
									fmt + exc.what());
		}
	}

	try {
		m_node.add_remote(remotes);
	} catch (ioremap::elliptics::error &exc) {
		const std::string fmt("failed to add remotes: ");
		throw std::system_error(std::make_error_code(std::errc::invalid_argument),
								fmt + exc.what());
	}

	{
		auto scn = args.as_object().at("success-copies-num", "any").as_string();

		if (scn == "any") {
			m_success_copies_num = ioremap::elliptics::checkers::at_least_one;
		} else if (scn == "quorum") {
			m_success_copies_num = ioremap::elliptics::checkers::quorum;
		} else if (scn == "all") {
			m_success_copies_num = ioremap::elliptics::checkers::all;
		} else {
			throw std::system_error(-1001, std::generic_category(), "unknown success-copies-num type");
		}
	}

	{
		dynamic_t::object_t timeouts = args.as_object().at("timeouts", dynamic_t::empty_object).as_object();
		m_timeouts.read   = timeouts.at("read", 5u).as_uint();
		m_timeouts.write  = timeouts.at("write", 5u).as_uint();
		m_timeouts.remove = timeouts.at("remove", 5u).as_uint();
		m_timeouts.find   = timeouts.at("find", 5u).as_uint();
	}

	dynamic_t::array_t groups = args.as_object().at("groups", dynamic_t::empty_array).as_array();
	if (groups.empty()) {
		throw std::system_error(-1001, std::generic_category(), "no groups has been specified");
	}

	std::transform(groups.begin(), groups.end(), std::back_inserter(m_groups), std::mem_fn(&dynamic_t::as_uint));

	m_session.set_groups(m_groups);
	m_session.set_exceptions_policy(ell::session::no_exceptions);
}

void elliptics_storage_t::read(const std::string &collection, const std::string &key, callback<std::string> cb) {
	auto result = m_read_latest ? async_read_latest(collection, key) : async_read(collection, key);
	result.connect([=](const ell::sync_read_result& results, const ell::error_info& error){
		if (error) {
			auto e = std::system_error(-error.code(), std::generic_category(), error.message());
			cb(make_exceptional_future<std::string>(e));
		} else {
			// this code mimics get_one sync call behaviour
			for(const auto& r : results) {
				if (r.status() == 0 && !r.data().empty()) {
					cb(make_ready_future(r.file().to_string()));
					return;
				}
			}
			auto e = std::system_error(std::make_error_code(std::errc::invalid_argument), "no valid results found");
			cb(make_exceptional_future<std::string>(e));
		}
	});
}

void elliptics_storage_t::write(const std::string &collection,
	const std::string &key,
	const std::string &blob,
	const std::vector<std::string> &tags,
	callback<void> cb
)
{
	auto result = async_write(collection, key, blob, tags);
	result.connect([=](const ell::sync_write_result& results, const ell::error_info &error){
		if (error) {
			auto e = std::system_error(-error.code(), std::generic_category(), error.message());
			cb(make_exceptional_future<void>(e));
		} else {
			cb(make_ready_future());
		}
	});
}

void elliptics_storage_t::find(const std::string &collection,
	const std::vector<std::string> &tags,
	callback<std::vector<std::string>> cb)
{
	auto ec = std::make_error_code(std::errc::not_supported);
	auto msg = "elliptics indexes support has been dropped out - use pg wrapper instead";
	cb(make_exceptional_future<std::vector<std::string>>(ec, msg));
}

void elliptics_storage_t::remove(const std::string &collection, const std::string &key, callback<void> cb)
{
	auto result = async_remove(collection, key);
	result.connect([=](const ell::sync_remove_result& results, const ell::error_info &error){
		if (error) {
			auto e = std::system_error(-error.code(), std::generic_category(), error.message());
			cb(make_exceptional_future<void>(e));
		} else {
			cb(make_ready_future());
		}
	});
}

ell::async_read_result elliptics_storage_t::async_read(const std::string &collection, const std::string &key)
{
	using namespace std::placeholders;

	COCAINE_LOG_DEBUG(m_log, "reading the '{}' object, collection: '{}'", key, collection);

	ell::session session = m_session.clone();
	session.set_namespace(collection.data(), collection.size());
	session.set_timeout(m_timeouts.read);

	return session.read_data(key, 0, 0);
}

ell::async_read_result elliptics_storage_t::async_read_latest(const std::string &collection, const std::string &key)
{
	using namespace std::placeholders;

	COCAINE_LOG_DEBUG(m_log, "reading the '{}' object, collection: '{}'", key, collection);

	ell::session session = m_session.clone();
	session.set_namespace(collection.data(), collection.size());
	session.set_timeout(m_timeouts.read);

	return session.read_latest(key, 0, 0);
}

ell::async_write_result elliptics_storage_t::async_write(const std::string &collection, const std::string &key, const std::string &blob, const std::vector<std::string> &tags)
{
	if (!tags.empty()) {
		COCAINE_LOG_ERROR(m_log, "elliptics indexes are not supported, discarding tags");
	}

	COCAINE_LOG_DEBUG(m_log, "writing the '{}' object, collection: '{}'", key, collection);

	ell::session session = m_session.clone();
	session.set_namespace(collection.data(), collection.size());
	session.set_filter(ioremap::elliptics::filters::all_with_ack);
	session.set_timeout(m_timeouts.write);
	session.set_checker(m_success_copies_num);

	return session.write_data(key, blob, 0);
}

ell::async_remove_result elliptics_storage_t::async_remove(const std::string &collection, const std::string &key)
{
	using namespace std::placeholders;

	COCAINE_LOG_DEBUG(m_log, "removing the '{}' object, collection: '{}'", key, collection);

	ell::session session = m_session.clone();
	session.set_namespace(collection.data(), collection.size());
	session.set_timeout(m_timeouts.remove);
	session.set_checker(m_success_copies_num);

	return session.remove(key);
}

ioremap::elliptics::async_read_result elliptics_storage_t::async_cache_read(const std::string &collection, const std::string &key)
{
	COCAINE_LOG_DEBUG(m_log, "cache reading the '{}' object, collection: '{}'", key, collection);

	ell::session session = m_session.clone();
	session.set_namespace(collection.data(), collection.size());
	session.set_ioflags(DNET_IO_FLAGS_CACHE | DNET_IO_FLAGS_CACHE_ONLY);
	session.set_timeout(m_timeouts.read);

	return session.read_data(key, 0, 0);
}

ioremap::elliptics::async_write_result elliptics_storage_t::async_cache_write(const std::string &collection, const std::string &key,
	const std::string &blob, int timeout)
{
	COCAINE_LOG_DEBUG(m_log, "cache writing the '{}' object, collection: '{}'", key, collection);

	ell::session session = m_session.clone();
	session.set_namespace(collection.data(), collection.size());
	session.set_ioflags(DNET_IO_FLAGS_CACHE | DNET_IO_FLAGS_CACHE_ONLY);
	session.set_timeout(m_timeouts.write);
	session.set_checker(m_success_copies_num);

	return session.write_cache(key, blob, timeout);
}

std::pair<ioremap::elliptics::async_read_result, elliptics_storage_t::key_name_map> elliptics_storage_t::async_bulk_read(
	const std::string &collection, const std::vector<std::string> &keys)
{
	COCAINE_LOG_DEBUG(m_log, "bulk reading, collection: '{}'", collection);

	ell::session session = m_session.clone();
	session.set_namespace(collection.data(), collection.size());
	session.set_timeout(m_timeouts.read);

	key_name_map keys_map;
	dnet_raw_id id;

	for (size_t i = 0; i < keys.size(); ++i) {
		session.transform(keys[i], id);
		keys_map[id] = keys[i];
	}

	return std::make_pair(session.bulk_read(keys), std::move(keys_map));
}

ioremap::elliptics::async_write_result elliptics_storage_t::async_bulk_write(const std::string &collection, const std::vector<std::string> &keys,
	const std::vector<std::string> &blobs)
{
	COCAINE_LOG_DEBUG(m_log, "bulk writing, collection: '{}'", collection);

	ell::session session = m_session.clone();
	session.set_namespace(collection.data(), collection.size());
	session.set_filter(ell::filters::all);
	session.set_timeout(m_timeouts.write);
	session.set_checker(m_success_copies_num);

	std::vector<dnet_io_attr> ios;
	ios.reserve(blobs.size());

	dnet_io_attr io;
	dnet_id id;
	memset(&io, 0, sizeof(io));
	dnet_empty_time(&io.timestamp);
	memset(&id, 0, sizeof(id));

	for (size_t i = 0; i < blobs.size(); ++i) {
		session.transform(keys[i], id);
		memcpy(io.id, id.id, sizeof(io.id));

		io.size = blobs[i].size();

		ios.push_back(io);
	}

	return session.bulk_write(ios, blobs);
}


}} /* namespace cocaine::storage */
