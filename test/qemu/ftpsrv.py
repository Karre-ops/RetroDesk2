from pyftpdlib.authorizers import DummyAuthorizer
from pyftpdlib.handlers import FTPHandler
from pyftpdlib.servers import FTPServer
import logging
logging.basicConfig(filename='/tmp/ftpsrv.log', level=logging.INFO)
a = DummyAuthorizer(); a.add_user('retro', 'geheim', '/app/conversations/6abeca1f8dde4e6407393f7a/qemu/ftproot', perm='elradfmw')
h = FTPHandler; h.authorizer = a; h.passive_ports = range(30000, 30100)
FTPServer(('127.0.0.1', 21), h).serve_forever()
