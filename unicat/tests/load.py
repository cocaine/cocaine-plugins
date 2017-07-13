from tornado import gen
from tornado import ioloop

from cocaine.services import Service
from cocaine.exceptions import ServiceError
from cocaine.exceptions import DisconnectionError


@gen.coroutine
def grant(srv, urls, cids, uids, flags):
    ch = yield srv.grant(urls, cids, uids, flags)
    result = yield ch.rx.get()
    raise gen.Return(result)

@gen.coroutine
def revoke(srv, urls, cids, uids, flags):
    ch = yield srv.revoke(urls, cids, uids, flags)
    result = yield ch.rx.get()
    raise gen.Return(result)

@gen.coroutine
def test_alter(unicat, items):
    yield [
        grant(unicat, items, [1,2,3], [3,4,5,6], 3),
        grant(unicat, items, [1,2,3], [3,4,5,6], 3),

        grant(unicat, items, [1,2,3], [3,4,5,6], 2),

        grant(unicat, items, [1,2,3], [3,4,5,6], 0),

        revoke(unicat, items, [3,4,5], [6,7,8], 0),

        revoke(unicat, items, [3, 4], [5,6], 1),
        revoke(unicat, items, [3, 4], [5,6], 1),

        revoke(unicat, items, [1, 2], [4,5], 0),
        revoke(unicat, items, [1, 2], [4,5], 0),
    ]


if __name__ == '__main__':
    unicat = Service('unicat')

    print('Testing alter methods for existent storage items...')

    items = [
        # Note: contains duplicate!
        ('storage', '', 't1'),
        ('storage', '', 't2'),
        ('storage', '', 't3'),
        ('storage', '', 't1') ]

    ioloop.IOLoop.current().run_sync(lambda: test_alter(unicat, items))

    print('Testing alter methods for existent unicorn items...')

    items = [
        # Note: contains duplicate!
        ('unicorn', '', '/z1'),
        ('unicorn', '', '/z2'),
        ('unicorn', '', '/z2'),
    ]

    ioloop.IOLoop.current().run_sync(lambda: test_alter(unicat, items))

    print('Testing alter methods for existent unicorn and storage items...')
    items = [
        # Note: contains duplicate!
        ('unicorn', '', '/z1'),
        ('unicorn', '', '/z2'),
        ('unicorn', '', '/z2'),
        ('storage', '', 't1'),
        ('storage', '', 't2'),
        ('storage', '', 't3'),
        ('storage', '', 't1')
    ]

    # while(True):
    #     try:
    #         ioloop.IOLoop.current().run_sync(lambda: test_alter(unicat, items))
    #     except DisconnectionError as err:
    #         print("service disconnected {}".format(err))
    #         continue
    #     except ServiceError as err:
    #         print("service error {}".format(err))

    ioloop.IOLoop.current().run_sync(lambda: test_alter(unicat, items))

    print('Testing alter methods for non-existent unicorn and storage items...')

    items = [
        # Note: contains duplicate!
        ('unicorn', '', '/z1.broken'),
        ('unicorn', '', '/z2.broken'),
        ('unicorn', '', '/z2.broken'),
        ('storage', '', 't1.broken'),
        ('storage', '', 't2.broken'),
        ('storage', '', 't3.broken'),
        ('storage', '', 't1.broken')
    ]

    # Warning: a lot of exceptions output, should be controlled by CLI flags someday
    # except_count = 0
    # try:
    #     ioloop.IOLoop.current().run_sync(lambda: test_alter(unicat, items))
    # except Exception:
    #     except_count += 1
    #     # TODO: compare error
    #     print('Expected error')

    # TODO: test for different service namings

    print('\nDone.\nHave a nice day!')
