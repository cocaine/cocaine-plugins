
import sys
import random

from cocaine.services import Service, Locator
from cocaine.exceptions import ChokeEvent

from tornado.ioloop import IOLoop
from tornado import gen


def call_app(name, event):
    
    @gen.coroutine
    def _do():

        app = Service(name)

        try:
          ch = yield app.enqueue(event)
          while True:
            yield ch.tx.write("testing testing 123")
            yield ch.tx.write("testing testing 123")
            yield ch.tx.close()
            response = yield ch.rx.get()
            print "response:", response
            response = yield ch.rx.get()
            print "response:", response
        except ChokeEvent:
          print "end"

    io = IOLoop.current()
    io.run_sync(_do)


argv = sys.argv

name, event = "echo", "ping"

for i,o in zip(xrange(100), argv):
  if o == "--name" or o == "-n":
    name = argv[i+1]
  elif o == "--event" or o == "-e":
    event = argv[i+1]

call_app(name, event)

