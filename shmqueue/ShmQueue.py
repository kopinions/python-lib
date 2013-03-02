import shmqueue

class ShmQueue(object):
    def __init__(self):
        self._shmqueue = shmqueue.CShmQueue()

    def init(self, path_to_file, size):
        return shmqueue.init(self._shmqueue, path_to_file, size)

    def enqueue(self, data):
        return shmqueue.enqueue(self._shmqueue, data)

    def dequeue(self, length=4096):
        return shmqueue.dequeue(self._shmqueue, length)

    def is_empty(self):
        return shmqueue.is_empty(self._shmqueue)

    def size(self):
        return shmqueue.size(self._shmqueue)

    #def empty(self):
    #return empty(self._shmqueue)

if __name__ == "__main__":
    sq = ShmQueue()
    sq.init("./setup.py", 1000)
    sq.enqueue("this is a test 1")
    sq.enqueue("this is a test 2")
    sq.enqueue("this is a test 3")
    sq.enqueue("this is a test 4")
    msg = sq.dequeue()
    print msg
    print sq.size()
    print sq.is_empty()
    msg = sq.dequeue()
    print msg
    print sq.size()
    print sq.is_empty()
    msg = sq.dequeue()
    print msg
    print sq.size()
    print sq.is_empty()
    msg = sq.dequeue()
    print msg
    print sq.size()
    print sq.is_empty()
