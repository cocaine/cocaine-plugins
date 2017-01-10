#!/usr/bin/env python

import time
time.sleep(2)

from cocaine.logger import Logger
from cocaine.worker import Worker

#for i in xrange(100):
#  print 80*"a"

log = Logger()

def echo(request, response):
    print "start the reqest"
#    log.info("start the request")
    while True:
      inc = yield request.read()
      print "write a chunk"
#      log.info("write a chunk")
      response.write(str(inc))
    print "close the request"
#      log.info("close the request")
    response.close()


def main():
    w = Worker()
    w.on("ping", echo)
    w.run()


if __name__ == '__main__':
    main()

