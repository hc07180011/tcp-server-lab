"""
IMPORTANT!!
This client only works on UNIX based machines.
"""

import os
import sys
import time
import socket

import logging
logger = logging.getLogger()
logger.setLevel(logging.DEBUG)


port = int(sys.argv[1])


def f1():
    s = socket.socket()
    s.connect(("live.api.xpacebbs.com", port))

    logger.info("f1 starts")

    buf = s.recv(1024) # initial ack
    logging.debug("f1 " + "<- " + buf.decode())

    s.send(b"put 1")
    logging.debug("f1 " + "-> " + "put 1")
    buf = s.recv(1024) # put ack
    logging.debug("f1 " + "<- " + buf.decode())

    s.send(b"get 1")
    logging.debug("f1 " + "-> " + "get 1")
    buf = s.recv(1024) # query results
    logging.debug("f1 " + "<- " + buf.decode())

    assert buf.decode() == "ack 1"

    s.send(b"get 2")
    logging.debug("f1 " + "-> " + "get 2")
    buf = s.recv(1024) # query results
    logging.debug("f1 " + "<- " + buf.decode())

    time.sleep(5)
    s.close()

    logger.info("f1 ends")

    exit(0)


def f2():
    s = socket.socket()
    s.connect(("live.api.xpacebbs.com", port))

    logger.info("f2 starts")

    buf = s.recv(1024) # initial ack
    logging.debug("f2 " + "<- " + buf.decode())

    s.send(b"put 2")
    logging.debug("f2 " + "-> " + "put 2")
    buf = s.recv(1024) # put ack
    logging.debug("f2 " + "<- " + buf.decode())

    s.send(b"get 2")
    logging.debug("f2 " + "-> " + "get 2")
    buf = s.recv(1024) # query results
    logging.debug("f2 " + "<- " + buf.decode())

    assert buf.decode() == "ack 1"

    s.send(b"get 1")
    logging.debug("f2 " + "-> " + "get 1")
    buf = s.recv(1024) # query results
    logging.debug("f2 " + "<- " + buf.decode())

    time.sleep(2)

    s.send(b"get 3")
    logging.debug("f2 " + "-> " + "get 3")
    buf = s.recv(1024) # query results
    logging.debug("f2 " + "<- " + buf.decode())

    s.send(b"get 4")
    logging.debug("f2 " + "-> " + "get 4")
    buf = s.recv(1024) # query results
    logging.debug("f2 " + "<- " + buf.decode())

    time.sleep(3)
    s.close()

    logger.info("f2 ends")

    exit(0)


def f3():
    s = socket.socket()
    s.connect(("live.api.xpacebbs.com", port))

    logger.info("f3 starts")

    buf = s.recv(1024) # initial ack
    logging.debug("f3 " + "<- " + buf.decode())

    time.sleep(1)

    s.send(b"put 3")
    logging.debug("f3 " + "-> " + "put 3")
    buf = s.recv(1024) # put ack
    logging.debug("f3 " + "<- " + buf.decode())

    s.send(b"get 3")
    logging.debug("f3 " + "-> " + "get 3")
    buf = s.recv(1024) # query results
    logging.debug("f3 " + "<- " + buf.decode())

    assert buf.decode() == "ack 1"

    s.send(b"get 2")
    logging.debug("f3 " + "-> " + "get 2")
    buf = s.recv(1024) # query results
    logging.debug("f3 " + "<- " + buf.decode())

    # already waited for 1 second, user `2` should be online
    assert buf.decode() == "ack 1"

    time.sleep(4)

    s.close()

    logger.info("f3 ends")

    exit(0)


if __name__ == "__main__":
    
    if not os.fork():
        f1()

    if not os.fork():
        f2()

    if not os.fork():
        f3()

    os.wait()
