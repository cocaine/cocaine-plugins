
import logging
import socket

from tornado import gen
from tornado.gen import coroutine

import tornado.ioloop
import tornado.iostream
import tornado.tcpserver

from cocaine.common import CocaineErrno

from session import Session
from protocol import conductor, primitive


from agent_util import generate_service_id, msgpack_packb, msgpack_unpacker

from isolate import Isolate

logging.basicConfig(level=logging.ERROR,
                    format='%(asctime)s %(name)-12s %(levelname)-8s %(message)s')



class ConductorSession(Session):

    def __init__(self, stream, loop=None, requests_cache=None):
        super(ConductorSession, self).__init__(stream, loop=loop)
        if requests_cache is not None:
            self.requests = requests_cache
        else:
            self.requests = {}

    def dispatch(self, channel, method, args):
        self.log.debug("got (cid, mid, args): (%s, %s, %s)", channel, method, args)

        try:
            
            if method == conductor.connect:
                future = self.on_connect(*args)

            elif method == conductor.spool:
                future = self.on_spool(*args)

            elif method == conductor.spawn:
                future = self.on_spawn(*args)

            elif method == conductor.terminate:
                future = self.on_terminate(*args)

            elif method == conductor.cancel:
                future = self.on_cancel(*args)

            else:
                self.log.error("unknown method[%d] (args %s)", method, args)
                return

            def _trap(f):
                self.log.debug("future trap")
                try:
                    r = f.result()
                    self.log.debug("coroutine result %s", r)
                    if r is not None:
                        self.stream.write(msgpack_packb([channel, primitive.value, [r]]))
                except Exception as err:
                    self.log.error("future.result() exception %s", err)

                    self.stream.write(msgpack_packb([channel, primitive.error,
                                                     [[42, CocaineErrno.EUNCAUGHTEXCEPTION], str(err)]]))

            self.io_loop.add_future(future, _trap)

        except Exception as err:
            self.log.error("failed to invoke %s %s", err, type(err))

    @coroutine
    def on_connect(self, *args):
        self.log.debug("connect")

    def get_cached_request(req_id, fn):
        if req_id in self.requests:
            req = self.requests[req_id]
        else:
            req = fn()
            self.requests[req_id] = req
        return req

    @coroutine
    def on_spool(self, req_id, name, profile):
        self.log.debug("spool%s", (req_id, name, profile))

        def ctr():
            isolate = Isolate.get(name, profile)
            req = isolate.spool(req_id, name, profile)
            return req

        req = self.get_cached_request(req_id, ctr)

        r = yield req.run()
        raise gen.Return(r)
    
    @coroutine
    def on_spawn(self, req_id, name, profile, arguments, environment):
        self.log.debug("spawn%s", (req_id, name, profile, arguments, environment))

        def ctr():
            isolate = Isolate.get(name, profile)
            req = isolate.spawn(req_id, name, profile, arguments, environment,
                                containers_cache = self.containers)
            return req

        req = self.get_cached_request(req_id, ctr)

        r = yield req.run()
        raise gen.Return(r)

    @coroutine
    def on_terminate(self, req_id, container_id):
        self.log.debug("terminate%s", (req_id, container_id))

        def ctr():
            if container_id in self.containers:
                req = self.containers[container_id].terminate()
                return req

        req = self.get_cached_request(req_id, ctr)
        if req:
            r = yield req.run()
            raise gen.Return(r)

    @coroutine
    def on_cancel(self, req_id):
        self.log.debug("cancel%s", (req_id,))

        if req_id in self.requests:
            req = self.requests[req_id]
            yield req.cancel()
        


class ConductorService(tornado.tcpserver.TCPServer):

    def __init__(*args, **kwargs):
        super(Service, self, *args, **kwargs)
        self.requests_cache = {}
    
    @coroutine
    def handle_stream(self, stream, address):
        session = ConductorSession(stream, loop=self.io_loop, requests_cache=self.requests_cache)


def main():
    host = '0.0.0.0'
    port = 6587

    service = ConductorService()
    service.listen(port, host)
    print("Listening on %s:%d..." % (host, port))

    tornado.ioloop.IOLoop.instance().start()


if __name__ == "__main__":
    main()

