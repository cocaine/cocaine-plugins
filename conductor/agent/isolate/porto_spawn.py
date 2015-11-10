
class Spawner(object):

    def __init__(self, name, profile, path, arguments, environment):


import subprocess
import random

from cocaine.services import Service, Locator

from tornado.ioloop import IOLoop
from tornado import gen

import os


agent_id = random.randint(1, 10**9)

conductor = Service("conductor")


@gen.coroutine
def dump_stream():

    ch = yield conductor.subscribe(agent_id)

    while True:
        rq = yield ch.rx.get()
        print "================"
        print "got request", rq

        yield process_request(rq)

            

@gen.coroutine
def process_request(rq0):

    rq = rq0[1]

    if rq.get("request", {}).get("action") == "spawn":

        _id, slave, args, env = int(rq["request"]["id"]), rq["isolate_params"]["slave"], rq["args"], rq["environment"]

        do_spawn(_id, slave, args, env)

        yield report_spawn_done(_id)


@gen.coroutine
def report_spawn_done(_id):

    print "report_spawn_done"
    ch = yield conductor.spawn_done(_id, [0,0], "done")
    yield ch.rx.get()
    print "reported"


PROCS = {}

def do_spawn(_id, path, args, env):

    stdout_pipe = args["--endpoint"] + "."+args["--uuid"]+".stdout.pipe"
    os.mkfifo(stdout_pipe)

    stdout_file = open(stdout_pipe, "w+")

    argv = [path]
    for k,v in args.iteritems():
        argv.append(k)
        argv.append(v)

    print "spawning slave"
    proc = subprocess.Popen(argv,
                            stdout=stdout_file,
                            #stdout=stdout_pipe,
                            stderr=subprocess.STDOUT)
    print "slave spawned"
    PROCS[_id] = proc

        

io = IOLoop.current()
io.run_sync(dump_stream)

spool_req = [
    123,
    {'request': {
        'action': 'spool',
        'image': 'something.blabla/echo2',
        'id': '123'}}]

spawn_req = [
    124,
    {'environment': {},
     'args': {'--protocol': '1',
              '--app': 'echo2',
              '--endpoint': '/var/run/cocaine/echo2.17006',
              '--locator': '127.0.1.1:10053',
              '--uuid': '718a1d3c-37cb-4e01-92c5-8a96d125372c'},
     'request': {'action': 'spawn', 'image': 'something.blabla/echo2', 'id': '124'},
     'isolate_params': {'slave': '/dvl/co/porto/plugins-build/plugins/conductor/agent/tests/apps/fork/run-app.sh', 'isolate': 'porto'}}]
        




