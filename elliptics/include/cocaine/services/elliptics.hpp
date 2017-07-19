/*
* 2013+ Copyright (c) Ruslan Nigmatullin <euroelessar@yandex.ru>
* Copyright 2014-2016 Evgeny Safronov <division494@gmail.com>
* All rights reserved.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU Lesser General Public License for more details.
*/

#ifndef COCAINE_SERVICE_ELLIPTICS_STORAGE_HPP
#define COCAINE_SERVICE_ELLIPTICS_STORAGE_HPP

#include <cocaine/api/service.hpp>
#include <cocaine/api/storage.hpp>
#include <cocaine/idl/storage.hpp>
#include <cocaine/rpc/protocol.hpp>

namespace cocaine { namespace io {

struct elliptics_tag;

struct elliptics {

struct cache_read {
	typedef elliptics_tag tag;

	static const char* alias() {
		return "cache_read";
	}

	typedef boost::mpl::list<
		/* Key namespace. Currently no ACL checks are performed, so in theory any app can read any
		   other app data without restrictions. */
		std::string,
		/* Key. */
		std::string
	> argument_type;

	typedef option_of<
		/* The stored value. Typically it will be serialized with msgpack, but it's not a strict
		   requirement. But as there's no way to know the format, try to unpack it anyway. */
		std::string
	>::tag upstream_type;
};

struct read_latest {
	typedef elliptics_tag tag;

	static const char* alias() {
		return "read_latest";
	}

	typedef boost::mpl::list<
		/* Key namespace. Currently no ACL checks are performed, so in theory any app can read any
		   other app data without restrictions. */
		std::string,
		/* Key. */
		std::string
	> argument_type;

	typedef option_of<
		/* The stored value. Typically it will be serialized with msgpack, but it's not a strict
		   requirement. But as there's no way to know the format, try to unpack it anyway. */
		std::string
	>::tag upstream_type;
};

struct cache_write {
	typedef elliptics_tag tag;

	static const char* alias() {
		return "cache_write";
	}

	typedef boost::mpl::list<
		/* Key namespace. */
		std::string,
		/* Key. */
		std::string,
		/* Value. Typically, it should be serialized with msgpack, so that the future reader could
		   assume that it can be deserialized safely. */
		std::string,
		/* Timeout. Life-time of the data, if not set it's unlimited. */
		io::optional_with_default<int, 0>
	> argument_type;
};

struct bulk_read {
	typedef elliptics_tag tag;

	static const char* alias() {
		return "bulk_read";
	}

	typedef boost::mpl::list<
		/* Key namespace. Currently no ACL checks are performed, so in theory any app can read any
		   other app data without restrictions. */
		std::string,
		/* Keys. */
		std::vector<std::string>
	> argument_type;

	typedef option_of<
		/* The stored values. Typically it will be serialized with msgpack, but it's not a strict
		   requirement. But as there's no way to know the format, try to unpack it anyway. */
		std::map<std::string, std::string>
	>::tag upstream_type;
};

struct bulk_write {
	typedef elliptics_tag tag;

	static const char* alias() {
		return "bulk_write";
	}

	typedef boost::mpl::list<
		/* Key namespace. */
		std::string,
		/* Keys. */
		std::vector<std::string>,
		/* Values. Typically, it should be serialized with msgpack, so that the future reader could
		   assume that it can be deserialized safely. */
		std::vector<std::string>
	> argument_type;

	typedef option_of<
		/* Write results. If write for some key fails errno can be accessed by the key. */
		std::map<std::string, int>
	>::tag upstream_type;
};

};

template<>
struct protocol<elliptics_tag> : public extends<storage_tag> {
	typedef boost::mpl::int_<
		1
	>::type version;

	typedef boost::mpl::list<
		elliptics::cache_read,
		elliptics::cache_write,
		elliptics::bulk_read,
		elliptics::read_latest
		// elliptics::bulk_write
	> messages;

	typedef elliptics scope;
};

}} // namespace cocaine::io

#endif /* COCAINE_SERVICE_ELLIPTICS_STORAGE_HPP */
