import os
import time
import random
import socket
import multiprocessing

import logging
logger = logging.getLogger()
logger.setLevel(logging.DEBUG)


host = "live.api.xpacebbs.com"
port = 80


def __get_status(user_id: int) -> bool:
    with open("status.lock", "r") as f:
        ids = [int(x) for x in f.read().split("\n") if x != ""]
    return user_id in ids


def __put_status(user_id: int, is_online: bool) -> None:
    with open("status.lock", "r") as f:
        ids = [int(x) for x in f.read().split("\n") if x != ""]
    if is_online and user_id not in ids:
        ids.append(user_id)
    elif (not is_online) and user_id in ids:
        ids.remove(user_id)
    with open("status.lock", "w") as f:
        f.write("\n".join([str(x) for x in ids]) + "\n")


def __login(lock: multiprocessing.Lock, s: socket.socket, id: int, user_id: int) -> None:
    lock.acquire()

    s.sendall(("put {}".format(user_id)).encode())
    logging.debug("{:08d} --> put {}".format(id, user_id))

    buf = s.recv(1024)
    logging.debug("{:08d} <-- {}".format(id, buf.decode()))

    __put_status(user_id, True)

    lock.release()

    assert buf.decode() == "ack"


def __logout(lock: multiprocessing.Lock, s: socket.socket, id: int, user_id: int) -> None:
    lock.acquire()

    __put_status(user_id, False)

    lock.release()


def __query(lock: multiprocessing.Lock, s: socket.socket, id: int, user_id: int) -> None:
    lock.acquire()

    s.sendall(("get {}".format(user_id)).encode())
    logging.debug("{:08d} --> get {}".format(id, user_id))

    buf = s.recv(1024)
    logging.debug("{:08d} <-- \'{}\'".format(id, buf.decode()))

    is_online_truth = __get_status(user_id)

    lock.release()

    if is_online_truth:
        assert buf.decode() == "ack 1"
    else:
        assert buf.decode() == "ack 0"


def f(lock, id):

    global_scale = 128

    s = socket.socket()
    s.connect((host, port))

    if random.random() ** 5 > 0.1:
        exit(1)

    logger.info("{:08d} starts".format(id))

    buf = s.recv(1024) # initial ack
    logging.debug("{:08d} <-- {}".format(id, buf.decode()))

    if random.random() ** 5 > 0.1:
        exit(1)

    __login(lock, s, id, id)

    if random.random() ** 5 > 0.1:
        __logout(lock, s, id, id)
        exit(1)

    for _ in range(global_scale):
        __query(lock, s, id, id + int(random.random() * global_scale))
        time.sleep(random.random())
        if random.random() ** 7 > 0.1:
            __logout(lock, s, id, id)
            exit(1)

    if random.random() ** 5 > 0.1:
        __logout(lock, s, id, id)
        exit(1)

    s.close()
    __logout(lock, s, id, id)

    logger.info("{:08d} ends".format(id))

    exit(0)


def test_basic():
    if os.path.exists("status.lock"):
        os.remove("status.lock")
    with open("status.lock", "w") as _: pass

    lock = multiprocessing.Lock()

    processes = list()
    for i in range(1024):
        processes.append(multiprocessing.Process(target=f, args=(lock, i + 1)))
        processes[-1].start()
        time.sleep(0.1)

    for p in processes:
        p.join()

    os.remove("status.lock")


if __name__ == '__main__':
    test_basic()