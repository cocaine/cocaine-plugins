/*
 * Copyright 2013+ Ruslan Nigmatullin <euroelessar@yandex.ru>
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

#include <cocaine/service.hpp>

#include <cocaine/dynamic.hpp>

#include "cocaine/tuple.hpp"

#define debug() if (1) {} else std::cerr
//#define debug() std::cerr << __PRETTY_FUNCTION__ << ": " << __LINE__ << " "

namespace cocaine {

namespace ph = std::placeholders;

using namespace ioremap;

elliptics_service_t::elliptics_service_t(context_t &context, asio::io_service &reactor, const std::string &name,
                                         const dynamic_t &args)
: api::service_t(context, reactor, name, args)
, dispatch<io::elliptics_tag>(name)
, m_storage(api::storage(context, args.as_object().at("source", "core").as_string()))
, m_elliptics(dynamic_cast<storage::elliptics_storage_t *>(m_storage.get())) {
	debug() << m_elliptics << std::endl;

	if (!m_elliptics) {
		throw std::system_error(-1, std::generic_category(),
		                        "to be able to use elliptics service, storage must be also elliptics");
	}

	on<io::storage::read  >(std::bind(&elliptics_service_t::read,   this, ph::_1, ph::_2));
	on<io::storage::write >(std::bind(&elliptics_service_t::write,  this, ph::_1, ph::_2, ph::_3, ph::_4));
	on<io::storage::remove>(std::bind(&elliptics_service_t::remove, this, ph::_1, ph::_2));
	on<io::storage::find  >(std::bind(&elliptics_service_t::find,   this, ph::_1, ph::_2));
	on<io::elliptics::cache_read >(std::bind(&elliptics_service_t::cache_read,  this, ph::_1, ph::_2));
	on<io::elliptics::cache_write>(std::bind(&elliptics_service_t::cache_write, this, ph::_1, ph::_2, ph::_3, ph::_4));
	on<io::elliptics::bulk_read  >(std::bind(&elliptics_service_t::bulk_read,   this, ph::_1, ph::_2));
	on<io::elliptics::read_latest>(std::bind(&elliptics_service_t::read_latest, this, ph::_1, ph::_2));
}

deferred<std::string> elliptics_service_t::read(const std::string &collection, const std::string &key) {
	debug() << "read, collection: " << collection << ", key: " << key << std::endl;
	deferred<std::string> promise;

	m_elliptics->async_read(collection, key).connect(std::bind(&on_read_completed,
		promise, ph::_1, ph::_2));

	return promise;
}

deferred<std::string> elliptics_service_t::read_latest(const std::string &collection, const std::string &key) {
	debug() << "read_latest, collection: " << collection << ", key: " << key << std::endl;
	deferred<std::string> promise;

	m_elliptics->async_read_latest(collection, key).connect(std::bind(&on_read_completed,
		promise, ph::_1, ph::_2));

	return promise;
}

deferred<void> elliptics_service_t::write(const std::string &collection, const std::string &key,
                                          const std::string &blob, const std::vector<std::string> &tags) {
	debug() << "write, collection: " << collection << ", key: " << key << std::endl;
	deferred<void> promise;

	m_elliptics->async_write(collection, key, blob, tags).connect(std::bind(&on_write_completed,
		promise, ph::_1, ph::_2));

	return promise;
}

deferred<std::vector<std::string>> elliptics_service_t::find(const std::string &collection,
                                                             const std::vector<std::string> &tags) {
	deferred<std::vector<std::string> > promise;
	auto ec = std::make_error_code(std::errc::not_supported);
	auto msg = "elliptics indexes support has been dropped out - use pg wrapper instead";
	promise.abort(ec, msg);

	return promise;
}

deferred<void> elliptics_service_t::remove(const std::string &collection, const std::string &key) {
	debug() << "remove, collection: " << collection << ", key: " << key << std::endl;
	deferred<void> promise;

	m_elliptics->async_remove(collection, key).connect(std::bind(&on_remove_completed,
		promise, ph::_1, ph::_2));

	return promise;
}

deferred<std::string> elliptics_service_t::cache_read(const std::string &collection, const std::string &key) {
	deferred<std::string> promise;

	m_elliptics->async_cache_read(collection, key).connect(std::bind(&on_read_completed,
		promise, ph::_1, ph::_2));

	return promise;
}

deferred<void> elliptics_service_t::cache_write(const std::string &collection, const std::string &key,
                                                const std::string &blob, int timeout) {
	deferred<void> promise;

	m_elliptics->async_cache_write(collection, key, blob, timeout).connect(std::bind(&on_write_completed,
		promise, ph::_1, ph::_2));

	return promise;
}

deferred<std::map<std::string, std::string>> elliptics_service_t::bulk_read(const std::string &collection,
                                                                            const std::vector<std::string> &keys) {
	deferred<std::map<std::string, std::string>> promise;

	auto result = m_elliptics->async_bulk_read(collection, keys);
	result.first.connect(std::bind(&on_bulk_read_completed,
		promise, std::move(result.second), ph::_1, ph::_2));

	return promise;
}

deferred<std::map<std::string, int>> elliptics_service_t::bulk_write(const std::string &collection,
                                                                     const std::vector<std::string> &keys,
                                                                     const std::vector<std::string> &blobs) {
	(void) collection;
	(void) keys;
	(void) blobs;

	deferred<std::map<std::string, int>> promise;

	promise.abort(std::error_code(ENOTSUP, std::generic_category()), "Not supported yet");

	return promise;
}

void elliptics_service_t::on_read_completed(nice_deferred<std::string> promise,
                                            const elliptics::sync_read_result &result,
                                            const elliptics::error_info &error) {
	if (error) {
		promise.abort(std::error_code(-error.code(), std::generic_category()), error.message());
	} else {
		promise.write(result[0].file().to_string());
	}
}

void elliptics_service_t::on_write_completed(nice_deferred<void> promise,
                                             const elliptics::sync_write_result & /*result*/,
                                             const elliptics::error_info &error) {
	if (error) {
		promise.abort(std::error_code(-error.code(), std::generic_category()), error.message());
	} else {
		promise.close();
	}
}

void elliptics_service_t::on_find_completed(nice_deferred<std::vector<std::string>> promise,
                                            const elliptics::sync_find_indexes_result &result,
                                            const elliptics::error_info &error) {
	if (error) {
		promise.abort(std::error_code(-error.code(), std::generic_category()), error.message());
	} else {
		promise.write(storage::elliptics_storage_t::convert_list_result(result));
	}
}

void elliptics_service_t::on_remove_completed(nice_deferred<void> promise,
                                              const elliptics::sync_remove_result & /*result*/,
                                              const elliptics::error_info &error) {
	if (error) {
		promise.abort(std::error_code(-error.code(), std::generic_category()), error.message());
	} else {
		promise.close();
	}
}

void elliptics_service_t::on_bulk_read_completed(nice_deferred<std::map<std::string, std::string>> promise,
                                                 const key_name_map &keys, const elliptics::sync_read_result &result,
                                                 const elliptics::error_info &error) {
	if (error) {
		promise.abort(std::error_code(-error.code(), std::generic_category()), error.message());
	} else {
		std::map<std::string, std::string> read_result;

		for (size_t i = 0; i < result.size(); ++i) {
			const auto &entry = result[i];
			const auto &id = reinterpret_cast<const dnet_raw_id &>(entry.command()->id);

			auto it = keys.find(id);

			if (it == keys.end()) {
				continue;
			}

			read_result[it->second] = entry.file().to_string();
		}

		promise.write(read_result);
	}
}

void elliptics_service_t::on_bulk_write_completed(nice_deferred<std::map<std::string, int>> promise,
                                                  const key_name_map &keys, const elliptics::sync_write_result &result,
                                                  const elliptics::error_info &error) {
	// Not implemented yet
	(void) promise;
	(void) keys;
	(void) result;
	(void) error;
}

}  // namespace cocaine
