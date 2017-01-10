

class conductor(object):
    connect = 0
    spool = 1
    spawn = 2
    terminate = 3
    cancel = 4

class primitive(object):
    value = 0
    error = 1


class Channel(object):

    def __init__(self, stream, channel_id, args):
        self.channel_id = channel_id
        self.args = args
    
    
