/*
 * Copyright (C) 2012+ Evgeniy Polyakov <zbr@ioremap.net>
 *
 * Licensed under the BSD 2-Clause License (the "License");
 * you may not use this file except in compliance with the License.
 */

#ifndef COCAINE_BINARY_SANDBOX_HPP
#define COCAINE_BINARY_SANDBOX_HPP

#include <cocaine/context.hpp>
#include <cocaine/logging.hpp>

#include <cocaine/api/sandbox.hpp>

extern "C" {
	struct binary_chunk {
		const char		*data;
		size_t			size;
	};

	struct binary_io;

	typedef void (* binary_write_fn)(struct binary_io *, const void *, size_t);

	struct binary_io {
		struct binary_chunk	chunk;
		binary_write_fn		write;
		void				*priv_io;
	};
}

namespace cocaine { namespace sandbox {

typedef void *(* init_fn_t)(logging::log_t *logger, const char *cfg, const size_t size);
typedef void (* cleanup_fn_t)(void *);
typedef int (* process_fn_t)(void *, struct binary_io *);

class binary_t: public api::sandbox_t {
	public:
		typedef api::sandbox_t category_type;

		binary_t(context_t& context, const std::string& name, const Json::Value& args, const std::string& spool);
		virtual ~binary_t();

		virtual std::shared_ptr<api::stream_t> invoke(const std::string& method, const std::shared_ptr<api::stream_t>& upstream);

		const logging::log_t& log() const {
			return *m_log;
		}

		std::unique_ptr<logging::log_t> m_log;
		process_fn_t m_process;
		void *m_handle;

	private:
		lt_dladvise m_advice;
		cleanup_fn_t m_cleanup;
		lt_dlhandle m_bin;
};

}}

#endif /* COCAINE_BINARY_SANDBOX_HPP */
