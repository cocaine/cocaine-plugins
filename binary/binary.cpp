/*
 * Copyright (C) 2012+ Evgeniy Polyakov <zbr@ioremap.net>
 *
 * This file is part of Cocaine.
 *
 * Cocaine is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * Cocaine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "binary.hpp"

#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>

#include <cocaine/api/stream.hpp>

using namespace cocaine;
using namespace cocaine::sandbox;

namespace fs = boost::filesystem;

binary_t::binary_t(context_t &context, const std::string &name, const Json::Value &args, const std::string &spool) :
    category_type(context, name, args, spool),
    m_log(new logging::log_t(context, cocaine::format("app/%1%", name))),
    m_process(NULL), m_cleanup(NULL)
{
	fs::path source(spool);

	if (lt_dlinit() != 0)
		throw configuration_error_t("unable to initialize binary loader");

	if (!fs::is_directory(source))
		throw configuration_error_t("binary loaded object must be unpacked into directory");

	Json::Value filename(args["name"]);
	if (!filename.isString())
		throw configuration_error_t("malformed manifest: args/name must be a string");

	source /= filename.asString();

	lt_dladvise_init(&m_advice);
	lt_dladvise_global(&m_advice);

	m_bin = lt_dlopenadvise(source.string().c_str(), m_advice);
	if (!m_bin) {
		COCAINE_LOG_ERROR(m_log, "unable to load binary object %s: %s", source.string(), lt_dlerror());
		lt_dladvise_destroy(&m_advice);
		throw configuration_error_t("unable to load binary object");
	}

	init_fn_t init = NULL;
	init = reinterpret_cast<init_fn_t>(lt_dlsym(m_bin, "initialize"));
	m_process = reinterpret_cast<process_fn_t>(lt_dlsym(m_bin, "process"));
	m_cleanup = reinterpret_cast<cleanup_fn_t>(lt_dlsym(m_bin, "cleanup"));

	if (!m_process || !m_cleanup || !init) {
		COCAINE_LOG_ERROR(m_log, "invalid binary loaded: init: %p, process: %p, cleanup: %p",
				init, m_process, m_cleanup);
		lt_dladvise_destroy(&m_advice);
		throw configuration_error_t("invalid binary loaded: not all callbacks are present");
	}

	Json::Value config(args["config"]);
	std::string cfg = config.toStyledString();

	m_handle = (*init)(m_log.get(), cfg.c_str(), cfg.size() + 1);
	if (!m_handle) {
		COCAINE_LOG_ERROR(m_log, "binary initialization failed");
		lt_dladvise_destroy(&m_advice);
		throw configuration_error_t("binary initialization failed");
	}

	COCAINE_LOG_INFO(m_log, "successfully initialized binary module from %s", source.string());
}

binary_t::~binary_t()
{
	m_cleanup(m_handle);
	lt_dladvise_destroy(&m_advice);
}

namespace {
	void binary_write(struct binary_io *__io, const void *data, size_t size)
	{
		api::stream_t * stream = static_cast<api::stream_t*>(__io->priv_io);
		stream->push(static_cast<const char*>(data), size);
	}
	
	struct downstream_t:
		public api::stream_t
	{
		downstream_t(binary_t * parent,
			         const std::shared_ptr<api::stream_t>& upstream):
			m_parent(parent),
			m_upstream(upstream)
		{ }

		virtual void push(const char * data, size_t size)
		{
			struct binary_io bio;
			int err;

			bio.chunk.data = data;
			bio.chunk.size = size;
			bio.priv_io = static_cast<void*>(m_upstream.get());
			bio.write = binary_write;

			COCAINE_LOG_INFO(m_parent->m_log, "downstream got chunk size: %llu", size);

			err = m_parent->m_process(m_parent->m_handle, &bio);

			if(err < 0) {
				COCAINE_LOG_ERROR(m_parent->m_log, "process failed: %d", err);
				m_upstream->error(invocation_error, "processing error: " + boost::lexical_cast<std::string>(err));
			}

			COCAINE_LOG_INFO(m_parent->m_log, "downstream processing done");
		}

		virtual void error(error_code code, const std::string& message)
		{
			COCAINE_LOG_ERROR(m_parent->m_log, "downstream got err: %d, %s; closing upstream", code, message);
			m_upstream->close();
		}

		virtual void close()
		{
			COCAINE_LOG_INFO(m_parent->m_log, "downstream got close; closing upstream");
			m_upstream->close();
		}

		binary_t * m_parent;
		std::shared_ptr<api::stream_t> m_upstream;
	};
}

std::shared_ptr<api::stream_t> binary_t::invoke(const std::string &method, const std::shared_ptr<api::stream_t>& upstream)
{
	return std::make_shared<downstream_t>(this, upstream);
}

extern "C" {
    void initialize(api::repository_t& repository) {
        repository.insert<binary_t>("binary");
    }
}
