import sys
import time
import socket
import multiprocessing


port = int(sys.argv[1])


def f(uid: int):
    s = socket.socket()
    s.connect(("127.0.0.1", port))
    print("{:03d} start".format(uid))

    print("{:03d}".format(uid), s.recv(1024))

    s.send(("put {}".format(uid)).encode())
    print("{:03d}".format(uid), s.recv(1024))

    if uid == 1:
        s.send(("get 1").encode())
    else:
        s.send(("get {}".format(uid - 1)).encode())
    print("{:03d}".format(uid), s.recv(1024))

    time.sleep(3)
    s.close()
    print("{:03d} end".format(uid))


if __name__ == "__main__":
    p = list()
    for i in range(10):
        time.sleep(0.1)
        p.append(multiprocessing.Process(target=f, args=(i + 1,)))
        p[-1].start()

    for pp in p:
        pp.join()
