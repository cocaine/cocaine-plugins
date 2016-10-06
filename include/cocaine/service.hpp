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

#ifndef COCAINE_ELLIPTICS_SERVICE_HPP
#define COCAINE_ELLIPTICS_SERVICE_HPP

#include <cocaine/api/service.hpp>
#include <cocaine/api/storage.hpp>
#include <cocaine/rpc/dispatch.hpp>

#include "cocaine/services/elliptics.hpp"

#include "storage.hpp"

namespace cocaine {

template <typename T> class nice_deferred {
public:
	typedef deferred<T> inner_type;

private:
	inner_type inner;

public:
	/* implicit */
	nice_deferred(inner_type inner)
	: inner(std::move(inner)) {}

	template <class... Args> void write(Args &&... args) {
		try {
			inner.write(std::forward<Args>(args)...);
		} catch (const std::exception &err) {
			// Eat.
		}
	}

	void abort(const std::error_code &ec, const std::string &reason) {
		try {
			inner.abort(ec, reason);
		} catch (const std::exception &err) {
			// Eat.
		}
	}
};

template <> class nice_deferred<void> {
public:
	typedef deferred<void> inner_type;

private:
	inner_type inner;

public:
	/* implicit */
	nice_deferred(inner_type inner)
	: inner(std::move(inner)) {}

	void close() {
		try {
			inner.close();
		} catch (const std::exception &err) {
			// Eat.
		}
	}

	void abort(const std::error_code &ec, const std::string &reason) {
		try {
			inner.abort(ec, reason);
		} catch (const std::exception &err) {
			// Eat.
		}
	}
};

class elliptics_service_t : public api::service_t, public dispatch<io::elliptics_tag> {
public:
	elliptics_service_t(context_t &context, asio::io_service &reactor, const std::string &name,
	                    const dynamic_t &args);

	const io::basic_dispatch_t &prototype() const { return *this; }

	deferred<std::string> read(const std::string &collection, const std::string &key);

	deferred<void> write(const std::string &collection, const std::string &key, const std::string &blob,
	                     const std::vector<std::string> &tags);

	deferred<std::vector<std::string>> find(const std::string &collection, const std::vector<std::string> &tags);

	deferred<void> remove(const std::string &collection, const std::string &key);

	deferred<std::string> cache_read(const std::string &collection, const std::string &key);

	deferred<void> cache_write(const std::string &collection, const std::string &key, const std::string &blob,
	                           int timeout);

	deferred<std::map<std::string, std::string>> bulk_read(const std::string &collection,
	                                                       const std::vector<std::string> &keys);

	deferred<std::map<std::string, int>> bulk_write(const std::string &collection,
	                                                const std::vector<std::string> &keys,
	                                                const std::vector<std::string> &blob);

	deferred<std::string> read_latest(const std::string &collection, const std::string &key);

private:
	typedef storage::elliptics_storage_t::key_name_map key_name_map;

	static void on_read_completed(nice_deferred<std::string> promise,
		const ioremap::elliptics::sync_read_result &result,
		const ioremap::elliptics::error_info &error);

	static void on_write_completed(nice_deferred<void> promise,
		const ioremap::elliptics::sync_write_result &result,
		const ioremap::elliptics::error_info &error);

	static void on_find_completed(nice_deferred<std::vector<std::string> > promise,
		const ioremap::elliptics::sync_find_indexes_result &result,
		const ioremap::elliptics::error_info &error);

	static void on_remove_completed(nice_deferred<void> promise,
		const ioremap::elliptics::sync_remove_result &result,
		const ioremap::elliptics::error_info &error);

	static void on_bulk_read_completed(nice_deferred<std::map<std::string, std::string> > promise,
		const key_name_map &keys,
		const ioremap::elliptics::sync_read_result &result,
		const ioremap::elliptics::error_info &error);

	static void on_bulk_write_completed(nice_deferred<std::map<std::string, int> > promise,
		const key_name_map &keys,
		const ioremap::elliptics::sync_write_result &result,
		const ioremap::elliptics::error_info &error);

	// NOTE: This will keep the underlying storage active, as opposed to the usual usecase when
	// the storage object is destroyed after the node service finishes its initialization.
	api::storage_ptr m_storage;
	storage::elliptics_storage_t *m_elliptics;
};

} // namespace cocaine

#endif // COCAINE_ELLIPTICS_SERVICE_HPP
