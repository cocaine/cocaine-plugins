
import sh
from .porto_utils import Fetcher, Spawner

class Isolate(object):
    
    def __init__(self, name, profile):
        self.name = name
        self.profile = profile

    def spool(self, req_id, name, profile):
        req = SpoolRequest(req_id, name, profile)
        return req

    def spawn(self, req_id, name, profile, path, arguments, environment):
        req = SpawnRequest(req_id, name, profile, path, arguments, environment)
        return req

class SpoolRequest(BasicRequest):

    def __init__(self, req_id, name, profile):
        super(SpoolRequest, self).__init__(req_id)
        self.name = name
        self.profile = profile

    @coroutine
    def run(self):
        Fetcher(name, registry).fetch()


class SpawnRequest(BasicRequest):

    def __init__(self, req_id, name, profile, path, arguments, environment):
        super(SpawnRequest, self).__init__(req_id)
        self.name = name
        self.profile = profile
        self.path = path
        self.arguments = arguments
        self.environment = environment

    @coroutine
    def run(self):
        Spawner(name, profile, path, arguments, environment).spawn()
        # create rw volume
        # create/open named pipe
        # initialize container:
        #     (id, appname, root_path)
        # container.start(argv, env, pipe=pipe_path)
        
        # result = { container_id: c.id(),
        #            stdout_path: c.stdout_path() }
        # Agent.containers[container_id] = container

class TerminateRequest(BasicRequest):
    
    @coroutine
    def run(self, container):
        yield container.do_terminate
        
def gen_uuid():

    return "a"*64
        
class Container(object):

    def __init__(self, appname, profile, path, arguments, environment, container_id=None):
        if container_id is None:
            self._id = gen_uuid()
        else:
            self._id = container_id

    def id():
        return _id

    @coroutine
    def terminate(self):
        req = TerminateRequest(self)
        return req

    @coroutine
    def do_terminate(self):
        # send porto signal
        # sh.portoctl("terminate")

