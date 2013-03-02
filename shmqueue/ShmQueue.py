import shmqueue

class ShmQueueError(Exception):pass

class ShmQueue(object):
    def __init__(self):
        self._shmqueue = shmqueue.CShmQueue()

    def init(self, path_to_file, size):
        try:
            shmqueue.init(self._shmqueue, path_to_file, size)
        except Exception, e:
            raise ShmQueueError(str(e))

    def enqueue(self, data):
        try:
            shmqueue.enqueue(self._shmqueue, data)
        except Exception, e:
            raise ShmQueueError(str(e))

    def dequeue(self, length=4096):
        try:
            msg = shmqueue.dequeue(self._shmqueue, length)
        except Exception, e:
            raise ShmQueueError(str(e))
        if msg is None:
            raise ShmQueueError("The share memory queue is empty")
        return msg

    def is_empty(self):
        try:
            is_empty = shmqueue.is_empty(self._shmqueue)
        except Exception, e:
            raise ShmQueueError(str(e))

    def size(self):
        try:
            size = shmqueue.size(self._shmqueue)
        except Exception, e:
            raise ShmQueueError(str(e))
        return size

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
