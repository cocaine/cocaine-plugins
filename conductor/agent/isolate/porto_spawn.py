

import logging
import json
import os
import uuid

import sh
import requests

log = logging.getLogger("agent.fetcher")
log.setLevel(logging.DEBUG)
logging.basicConfig(level=logging.DEBUG) 

from porto_config import config
from porto_fetch import Fetcher

class Spawner(object):

    def __init__(self, name, registry, tag="latest"):
        self.name = name
        self.image = name # temporary alias, until refactored out
        self.tag = tag
        self.registry = registry

        self.fetcher = Fetcher(name, registry, tag)

    def spawn():
        volume_path = self.fetcher.create_volume(rw=True)

        # portoctl exec ubuntu-precise virt_mode="os" command="/bin/bash" hostname="" env="$ENVIRON; " bind="$BIND;" cwd="/" root=$root


        sh.portoctl("exec", container_name, 'virt_mode="os"', 'command=/'+slave, 
        

if __name__ == "__main__":
    s = Spawner("coke-example-128_v0-1-2-cocaine-9__v012", "registry.ape.yandex.net")
    s.spawn(rw=True)
    
