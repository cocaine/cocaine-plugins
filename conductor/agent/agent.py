
import random

from cocaine.services import Service, Locator

from tornado.ioloop import IOLoop
from tornado import gen


@gen.coroutine
def dump_stream():

    agent_id = random.randint(1, 10**9)

    conductor = Service("conductor")


    ch = yield conductor.subscribe(agent_id)

    while True:
        rq = yield ch.rx.get()
        print "================"
        print "got request", rq


io = IOLoop.current()
io.run_sync(dump_stream)

