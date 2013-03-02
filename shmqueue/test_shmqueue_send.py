#!/usr/bin/env python
import ShmQueue
if __name__ == "__main__":
    sq = ShmQueue.ShmQueue()
    sq.init("/tmp", 1000)
    sq.enqueue("this is a test 1")
    sq.enqueue("this is a test 2")
    sq.enqueue("this is a test 3")
    sq.enqueue("this is a test 4")
    print "Send success"
