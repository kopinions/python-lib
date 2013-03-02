from distutils.core import setup, Extension

setup(
      name = "shmqueue",
      py_modules = ["ShmQueue"],
      version = "0.0.1",
      description = "A share memory queue, only used on linux like platform",
      author = "magicjksun",
      license = "LGPL",
      url = "sjkyspa@gmail.com",
      ext_modules=[Extension("shmqueue", sources=["shmqueue.cpp", "buffer_queue.cpp"])],
      scripts=["test_shmqueue_recv.py", "test_shmqueue_send.py"]
      )
