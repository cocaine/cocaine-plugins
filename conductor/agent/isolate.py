

from .isolate import porto

class BasicRequest(object):

    def __init__(self, req_id):
        self._done = False
        self._cancelled = False

    def run_once():
        if not self._done and not self._cancelled:
            return self.run()

    @coroutine
    def run():
        pass
        
    def done(self):
        if not self._done and not self._cancelled:
            self._done = True

    @coroutine
    def cancel(self):
        pass


class Isolate(object):

    registry = {}

    @classmethod
    def get(cls, name, profile):
      return porto.Isolate(name, profile)
        

