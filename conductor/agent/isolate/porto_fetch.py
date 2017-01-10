

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

class Fetcher(object):

    def __init__(self, name, registry, tag="latest"):
        self.name = name
        self.image = name # temporary alias, until refactored out
        self.tag = tag
        self.registry = registry
    
    def create_volume(self, rw=False, layers=None):

        if layers is None:
            layers = self.fetch_volume_layers()

        if rw:
            writable_layer = "%s-%s" % (self.name, uuid.uuid4().hex)
            log.debug("creating extra writable layer for volume, %s", writable_layer)
            sh.mkdir("-p", "%s/%s" % (config.porto_layers, writable_layer))
            layers.insert(0, writable_layer)
            volume_path = "%s/%s" % (config.volumes_base, writable_layer)
        else:
            volume_path = "%s/%s:%s" % (config.volumes_base, self.image, self.tag)
            
        sh.mkdir("-p", volume_path)
        r = sh.portoctl("vcreate", volume_path, "layers="+(";".join(layers)))
        log.debug("volume created: %s", volume_path)
        return volume_path

    def fetch_volume_layers(self):
        ancestry_path = "%s/%s.json" % (config.spool_dir, self.name)

        try:
            with open(ancestry_path) as f:
                layers = json.load(f)
                
        except Exception:
            layers = self.fetch_layers(self.image, self.tag)

            for _id in reversed(layers):
                self.import_layer_whands(_id)

            with open(ancestry_path, "w") as f:
                json.dump(layers, f)
            
        return layers

    def import_layer_whands(self, _id):
        image_tar = "%s/%s.tar" % (config.spool_dir, _id)

        PLACE = "%s/%s" % (config.porto_layers, _id)

        log.debug("importing %s layer", _id)
        sh.mkdir("-p", PLACE)
        sh.tar("xf", image_tar, "-C", PLACE)

    def import_layer_wporto(self, _id):
        image_tar = IMAGES_DIR+"/%s.tar"%_id

        log.debug("importing %s layer", _id)
        try:
            sh.portoctl("layer","-I", _id, image_tar)
        except Exception as e:
            if e.stderr.find("layer already exists") != -1:
                pass
            else:
                raise

    def fetch_layers(self, image, tag="latest"):
        registry = self.registry

        tag_url = "http://%(registry)s/v1/repositories/%(image)s/tags/%(tag)s" % {
            "registry": registry,
            "image": image,
            "tag": tag
        }

        log.info("fetching latest tag at %s", tag_url)
        image_id = json.loads(requests.get(tag_url).text)

        log.info("latest tag is %s", image_id)


        ancestry_url = "http://%(registry)s/v1/images/%(id)s/ancestry" % {
            "registry": registry,
            "id": image_id
        }

        log.info("fetching %s's ancestry at %s", image_id, ancestry_url)

        self.ancestry = json.loads(requests.get(ancestry_url).text)
        log.info("fetched ancestry: %s", self.ancestry)

        manifest_url = "http://%(registry)s/v1/images/%(id)s/json" % {
            "registry": registry,
            "id": image_id
        }

        log.info("fetching %s's manifest at %s", image_id, manifest_url)
        self.manifest = json.loads(requests.get(manifest_url).text)
        log.info("fetched manifest %s", self.manifest)

        for a in reversed(self.ancestry):
            self.fetch_layer(a)

        return self.ancestry


    def fetch_layer(self, _id):
        registry = self.registry

        layer_url = "http://%(registry)s/v1/images/%(id)s/layer" % {
            "registry": registry,
            "id": _id
        }

        fs_image_path = "%s/%s.tar" % (config.spool_dir, _id)

        log.debug("fetching %s/%s (%s)", registry, _id, layer_url)
        log.debug("    to %s", fs_image_path)

        size = maybe_filesize(fs_image_path)

        headers = None
        target_size = 0
        if size:
            resp = requests.head(layer_url)
            target_size = int(resp.headers["content-length"])
            log.debug("detected target size %s", target_size)

        if size == target_size and size != 0:
            return
        elif size == 0 or size < target_size:
            headers = {'Range': 'bytes=%d-' % size}
            wf = open(fs_image_path, 'ab')
        else:
            headers = {}
            log.warning("WARNING: downloaded layer larger than original, overwriting")
            wf = open(fs_image_path, 'wb')

        resp = requests.get(layer_url, headers=headers, stream=True)

        log.debug("resp, headers: %s", (resp, resp.headers))

        with wf:
            for chunk in resp.iter_content(chunk_size=65*1024):
                if chunk:
                    wf.write(chunk)
                    wf.flush()


def maybe_filesize(path):
    try:
        st = os.stat(path)
        return st.st_size
    except OSError as e:
        return 0

if __name__ == "__main__":
    f = Fetcher("coke-example-128_v0-1-2-cocaine-9__v012", "registry.ape.yandex.net")
    #f = Fetcher("ubuntu", "registry.ape.yandex.net", tag="precise")
    #f.fetch_volume_layers()
    f.create_volume(rw=True)
