import time
from cocaine.services import Service
from cocaine.decorators import coroutine


from tornado import gen
from tornado.ioloop import IOLoop

import sys

def do_control():

    io = IOLoop.current()
    
    io.run_sync(execute)

    print "uploaded ok"


@coroutine
def execute():
    app = Service("echo2")
    if 1 < len(sys.argv):
       target = int(sys.argv[1])
    else:
       target = 5
        
    ch = yield app.control()
    yield ch.tx.write(target)
    time.sleep(2000)



do_control()

