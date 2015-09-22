
import random

from cocaine.services import Service, Locator

from tornado.ioloop import IOLoop
from tornado import gen


def call_app(name, event):
    
    @gen.coroutine
    def _do():

        agent_id = random.randint(1, 10**9)

        conductor = Service(name)

        ch = yield conductor.enqueue(event)

        while True:
            rq = yield ch.rx.get()
            print "================"
            print "got request", rq

    io = IOLoop.current()
    io.run_sync(_do)


call_app("app123", "bla")


