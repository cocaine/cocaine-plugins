

import hashlib
import sys
import time
from functools import partial

import msgpack


if sys.version_info[0] == 2:
    msgpack_packb = msgpack.packb
    msgpack_unpackb = msgpack.unpackb
    msgpack_unpacker = msgpack.Unpacker
else:  # pragma: no cover
    # py3: msgpack by default unpacks strings as bytes.
    # Make it to unpack as strings for compatibility.
    msgpack_packb = msgpack.packb
    msgpack_unpackb = partial(msgpack.unpackb, encoding="utf8")
    msgpack_unpacker = partial(msgpack.Unpacker, encoding="utf8")


if sys.version_info[0] == 2:
    def valid_chunk(chunk):
        return isinstance(chunk, (str, unicode, bytes))

    def generate_service_id(self):
        return hashlib.md5("%d:%f" % (id(self), time.time())).hexdigest()[:15]
else:
    def valid_chunk(chunk):
        return isinstance(chunk, (str, bytes))

    def generate_service_id(self):
        hashed = "%d:%f" % (id(self), time.time())
        return hashlib.md5(hashed.encode("utf-8")).hexdigest()[:15]

