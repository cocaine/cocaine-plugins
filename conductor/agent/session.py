
import logging
import socket

import tornado.gen
from tornado.gen import coroutine

import tornado.ioloop
import tornado.iostream
import tornado.tcpserver

from cocaine.common import CocaineErrno

from protocol import conductor, primitive

from agent_util import generate_service_id, msgpack_packb, msgpack_unpacker


class Session(object):
    session_id = 0

    def __init__(self, stream, loop=None):
        super(Session, self).__init__()

        self.id = Session.session_id
        Session.session_id += 1

        self.log = logging.getLogger("agent.session[%s]" % self.id)
        self.log.setLevel(logging.DEBUG)

        if loop == None:
            self.io_loop = tornado.ioloop.IOLoop.instance()
        else:
            self.log.debug("initializing session with loop")
            self.io_loop = loop

        self.max_seen_channel_id = 0

        self.buffer = msgpack_unpacker()

        self.stream = stream

        self.on_socket_connect()
    
    def dispatch(self, channel, message_type, args):
        pass

    def on_socket_close(self, *args):
        self.log.debug("client disconnected")

    def on_socket_read(self, read_bytes):
        self.log.debug("read %.300s", read_bytes)
        self.buffer.feed(read_bytes)
        for msg in self.buffer:
            self.log.debug("unpacked: %.300s", msg)
            try:
                channel, message_type, payload = msg[:3]  # skip extra fields
                self.log.debug("%s, %d, %.300s", channel, message_type, payload)
            except Exception as err:
                self.log.error("malformed message: `%s` %s", err, str(msg))
                continue

            if self.max_seen_channel_id < channel:
                self.max_seen_channel_id = channel
                rx = self.dispatch(channel, message_type, payload)
            else:
                self.log.warning("duplicate or unknown session number: `%d`", channel)


    def on_socket_connect(self):

        self.stream.socket.setsockopt(
            socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        self.stream.socket.setsockopt(
            socket.IPPROTO_TCP, socket.SO_KEEPALIVE, 1)

        raddr = 'closed'
        try:
            raddr = '%s:%d' % self.stream.socket.getpeername()
        except Exception:
            pass
        self.log.debug('new, %s' % raddr)

        self.stream.read_until_close(callback=self.on_socket_close,
                                     streaming_callback=self.on_socket_read)
