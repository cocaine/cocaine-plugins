/*
 * Copyright 2013+ Ruslan Nigmatullin <euroelessar@yandex.ru>
 * Copyright 2011-2012 Andrey Sibiryov <me@kobology.ru>
 * Copyright 2014-2016 Evgeny Safronov <division494@gmail.com>
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

#ifndef COCAINE_ELLIPTICS_STORAGE_HPP
#define COCAINE_ELLIPTICS_STORAGE_HPP

#include <blackhole/record.hpp>

#include <cocaine/api/service.hpp>
#include <cocaine/api/storage.hpp>
#include <cocaine/rpc/dispatch.hpp>

#include "elliptics/cppdef.h"

namespace cocaine {
class elliptics_service_t;
}

namespace cocaine { namespace storage {

class elliptics_storage_t : public api::storage_t {
public:
	typedef api::storage_t category_type;
	typedef std::map<dnet_raw_id, std::string, ioremap::elliptics::dnet_raw_id_less_than<>> key_name_map;

	elliptics_storage_t(context_t &context, const std::string &name, const dynamic_t &args);

	void read(const std::string &collection, const std::string &key, callback<std::string> cb);
	void write(const std::string &collection, const std::string &key, const std::string &blob,
		const std::vector<std::string> &tags, callback<void> cb);
	void find(const std::string &collection, const std::vector<std::string> &tags, callback<std::vector<std::string>> cb);
	void remove(const std::string &collection, const std::string &key, callback<void> cb);

protected:
	ioremap::elliptics::async_read_result async_read(const std::string &collection, const std::string &key);
	ioremap::elliptics::async_write_result async_write(const std::string &collection, const std::string &key,
		const std::string &blob, const std::vector<std::string> &tags);
	ioremap::elliptics::async_remove_result async_remove(const std::string &collection, const std::string &key);
	ioremap::elliptics::async_read_result async_cache_read(const std::string &collection, const std::string &key);
	ioremap::elliptics::async_write_result async_cache_write(const std::string &collection, const std::string &key,
		const std::string &blob, int timeout);
	std::pair<ioremap::elliptics::async_read_result, key_name_map> async_bulk_read(const std::string &collection, const std::vector<std::string> &keys);
	ioremap::elliptics::async_write_result async_bulk_write(const std::string &collection, const std::vector<std::string> &keys,
		const std::vector<std::string> &blobs);
	ioremap::elliptics::async_read_result async_read_latest(const std::string &collection, const std::string &key);

private:
	std::string id(const std::string &collection, const std::string &key) {
		return collection + '\0' + key;
	}
	std::unique_ptr<logging::logger_t> make_elliptics_logger(const std::string& name) const;
	ioremap::elliptics::session prepare_session(const std::string collection, int timeout);

private:
	context_t &m_context;
	std::unique_ptr<logging::logger_t> m_log;

	// Perform read latest operation on read request.
	bool m_read_latest;
	dnet_config m_config;
	ioremap::elliptics::node m_node;
	ioremap::elliptics::session m_session;
	ioremap::elliptics::result_checker m_success_copies_num;

	struct {
		int read;
		int write;
		int remove;
		int find;
	} m_timeouts;

	std::vector<int> m_groups;

	friend class cocaine::elliptics_service_t;
};

}} /* namespace cocaine::storage */

#endif
