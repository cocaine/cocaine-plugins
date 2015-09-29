
import logging
import json
import os
import sh

import requests


log = logging.getLogger("agent.fetcher")
log.setLevel(logging.DEBUG)
#logging.basicConfig(level=logging.DEBUG) 


IMAGES_DIR="/dvl/co/porto/plugins-build/plugins/conductor/agent/images"

def create_volume(registry, image, tag="latest"):

    layers = fetch_layers(registry, image, tag)

    for _id in reversed(layers):
        import_layer_whands(_id)

    volume_path = "/place/cocaine_volumes/%s:%s"%(image,tag)
    sh.mkdir("-p", volume_path)
    print sh.portoctl("vcreate", volume_path, "layers="+(";".join(layers)))
    #v = create_volume(layers)

def import_layer_whands(_id):
    image_tar = IMAGES_DIR+"/%s.tar"%_id

    PLACE = "/place/porto_layers/%s"%_id

    print "importing %s layer"%(_id)
    sh.mkdir("-p", PLACE)
    sh.tar("xf", image_tar, "-C", PLACE)

def import_layer_wporto(_id):
    image_tar = IMAGES_DIR+"/%s.tar"%_id

    print "importing %s layer"%(_id)
    try:
        sh.portoctl("layer","-I", _id, image_tar)
    except Exception as e:
        if e.stderr.find("layer already exists") != -1:
            pass
        else:
            raise

def fetch_layers(registry, image, tag="latest"):


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
    
    ancestry = json.loads(requests.get(ancestry_url).text)
    log.info("fetched ancestry: %s", ancestry)

    manifest_url = "http://%(registry)s/v1/images/%(id)s/json" % {
        "registry": registry,
        "id": image_id
    }

    log.info("fetching %s's manifest at %s", image_id, manifest_url)
    manifest = json.loads(requests.get(manifest_url).text)
    log.info("fetched manifest %s", manifest)

    for a in reversed(ancestry):
        fetch_layer(registry, a)

    return ancestry
    

def fetch_layer(registry, _id):

    layer_url = "http://%(registry)s/v1/images/%(id)s/layer" % {
        "registry": registry,
        "id": _id
    }

    fs_image_path = IMAGES_DIR + "/" + _id + ".tar"

    print "fetching %s/%s (%s)" % (registry, _id, layer_url)
    print "    to %s"%fs_image_path

    size = maybe_filesize(fs_image_path)

    headers = None
    target_size = 0
    if size:
        resp = requests.head(layer_url)
        target_size = int(resp.headers["content-length"])

    if size == target_size:
        return
    elif size < target_size:
        headers = {'Range': 'bytes=%d-' % size}
        wf = open(fs_image_path, 'ab')
    else:
        headers = {}
        print "WARNING: downloaded layer larger than original, overwriting"
        wf = open(fs_image_path, 'wb')
            
    resp = requests.get(layer_url, headers=headers, stream=True)

    print "rq, headers", resp, resp.headers
    
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



#create_container("registry.ape.yandex.net", "coke-example-128_v0-1-2-cocaine-9__v012")

create_volume("registry.ape.yandex.net", "ubuntu", "precise")
