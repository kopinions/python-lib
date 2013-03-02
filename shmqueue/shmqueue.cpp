#include <Python.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdexcept>
#include <iostream>
#include <assert.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <iostream>
#include "buffer_queue.h"

using namespace std;


// create a new exception type which is inherited from exception
static PyObject* BufferFull;

class CShmQueue {
public:
	CShmQueue(){
		_shmid = -1;
		_semid = -1;
		_shmkey = -1;
		_semkey = -1;
		_queuesize = 0;
	}
	~CShmQueue(){}

public:
	enum HeadStat {
		Stat_Std = 0,		
		Stat_Close_peer,
		Stat_Close_timeout,	
		Stat_Close_error,	
		Stat_Close_Server,	
		Stat_Close_SendOver, 
		Stat_Accept,			
		Stat_Data_SendAndClose, 
		Stat_Manager,		
		Stat_Close_Manager		
	};

	struct Head {
		unsigned sock_index;
		struct timeval sock_create;
		struct timeval request;
		struct timeval response;
		unsigned src_ip;
		unsigned short src_port;
		unsigned short stat;
	};
    

	void init(key_t key,key_t semkey,unsigned long queuesize) throw(std::runtime_error);
	// 使用文件生成key
	void init(const std::string& shmkeyfile,const std::string& semkeyfile,unsigned long queuesize) throw(std::runtime_error);
	// throw buffer_full when queue package length > buffersize
	bool Dequeue(char *buffer,unsigned& buffersize) throw(buffer_full, std::runtime_error);
	// throw buffer_full when queue is full
	void Enqueue(const char *buffer,unsigned len) throw(buffer_full, std::runtime_error);
	bool IsEmpty();
    unsigned int count() ;

    /**
     * unsigned转string
     */
    inline static string u2s(const unsigned u)
    {
        char sTmp[64] ={0};
        sprintf(sTmp, "%u", u);
        return string(sTmp);
    } 
	static string ipv42s(unsigned ip) 
	{
        std::string s; 
        s+=u2s(((unsigned char *)&ip)[0]); 
        s+=".";
        s+=u2s(((unsigned char *)&ip)[1]);
        s+=".";
        s+=u2s(((unsigned char *)&ip)[2]); 
        s+=".";
        s+=u2s(((unsigned char *)&ip)[3]); 
        return s;
    }


    static key_t ftok(const std::string &file_name);
        
private:
	void lock()throw(std::runtime_error);
	void unlock()throw(std::runtime_error);

private:
	CBufferQueue _queue;
	long _shmid;
	long _semid;
	key_t _shmkey;
	key_t _semkey;
	unsigned long _queuesize;
};

inline  key_t CShmQueue::ftok(const std::string &file_name)
{
    struct stat st;
    key_t key;

    if (stat (file_name.c_str(), &st) < 0)
        return (key_t) -1;

    int proj_id = (st.st_ino & 0x7f0000) >> 16;
    key = ((st.st_ino & 0xffff) | ((st.st_dev & 0xff) << 16)
	     | ((proj_id & 0xff) << 24));

    return key;
}

inline void CShmQueue::init(const std::string& shmkeyfile,const std::string& semkeyfile, unsigned long queuesize) throw(std::runtime_error)
{
	key_t key = ftok(shmkeyfile.c_str()); 
	if(key < 0) throw std::runtime_error(std::string("CShmQueue::init shmkeyfile invalid:")+shmkeyfile);
	key_t semkey = ftok(semkeyfile.c_str()); 
	if(semkey < 0) throw std::runtime_error(std::string("CShmQueue::init semkeyfile invalid:")+semkeyfile);
	init(key,semkey,queuesize);
}

inline void CShmQueue::init(key_t key,key_t semkey, unsigned long queuesize) throw(std::runtime_error)
{
	assert(queuesize>0);
	assert(key>0);
	assert(semkey>0);
	bool binit = false;
	_queuesize = queuesize;
	_shmkey = key;
	_semkey = semkey;
	_shmid = shmget(_shmkey, _queuesize, IPC_CREAT|IPC_EXCL|0666);
	if( _shmid < 0 ) {
		if(errno != EEXIST ) {
			throw std::runtime_error(std::string("CShmQueue::init: create shm fail:")+strerror(errno));
		}
		_shmid = shmget(_shmkey, _queuesize, 0666);
		if( _shmid < 0 ) {
			throw std::runtime_error(std::string("shmget attach fail:")+strerror(errno));
		}
	} else {
		binit = true;
	}
	//cout << "alloc shm succ, key:" << _shmkey << " id:" << _shmid << " size:" << _queuesize << endl;

	_semid = semget(_semkey, 1, IPC_CREAT | IPC_EXCL | 0666);
	if (_semid < 0) { 
		if (errno != EEXIST) {
			throw std::runtime_error(std::string("CShmQueue::init: create sem fail:")+strerror(errno));
		}
		_semid = semget(_semkey, 1, 0666);
		if( _semid < 0 ) {
			throw std::runtime_error(std::string("CShmQueue::init: attach sem fail:")+strerror(errno));
		}
	} else {
		// init sem
		unsigned short* init_array = new unsigned short[1];
		init_array[0] = 1;
		int ret = semctl(_semid, 0, SETALL, init_array);
		delete [] init_array;
		if(ret < 0) {
			throw std::runtime_error(std::string("CShmQueue::init: semctl sem fail:")+strerror(errno));
		}
	}
	//cout << "alloc sem succ, key:" << _semkey << " id:" << _semid << endl;

	char *buffer = (char *)shmat( _shmid, NULL, 0);
	if((long)buffer == -1) throw std::runtime_error(std::string("CShmQueue::Init shmat fail:")+strerror(errno));

	if(binit) _queue.create(buffer, _queuesize);
	else _queue.attach(buffer, _queuesize);
}

inline bool CShmQueue::Dequeue(char *buffer,unsigned& buffersize) throw(buffer_full, std::runtime_error)
{
	lock();
	try {
		if(_queue.dequeue(buffer, buffersize)) {
			unlock();
			return true;
		} 
		unlock();
		return false;
	} catch(buffer_full& e) {
		unlock();
		throw e;
	}
}

inline void CShmQueue::Enqueue(const char *buffer,unsigned len) throw(buffer_full,std::runtime_error)
{
	lock();
	try {
		_queue.enqueue(buffer, len);
		unlock();
	} catch(buffer_full& e) {
		unlock();
		throw e;
	} 
}

inline void CShmQueue::lock() throw(std::runtime_error)
{
	for(;;)
    {
		struct sembuf sops;
		sops.sem_num = 0;
		sops.sem_op = -1;
		sops.sem_flg = SEM_UNDO;

		int ret = semop(_semid, &sops, 1);
		if(ret<0) 
        {
			if(errno == EINTR) 
            {
				cerr << "CShmQueue:lock EINTR" << endl;
				continue;
	        }
			else 
            {
				throw std::runtime_error(std::string("CShmQueue lock fail:")+strerror(errno));
			}
		}
        else 
        {
			break;
		}
	}
}

inline void CShmQueue::unlock() throw(std::runtime_error)
{
	for(;;) 
    {
		struct sembuf sops;
		sops.sem_num = 0;
		sops.sem_op = 1;
		sops.sem_flg = SEM_UNDO;

		int ret = semop(_semid, &sops, 1);
		if(ret<0) 
        {
			if(errno == EINTR) 
            {
				cerr << "CShmQueue:unlock EINTR" << endl;
				continue;
			}
			else 
            {
				throw std::runtime_error(std::string("CShmQueue:unlock fail:")+strerror(errno));
			}
		} 
        else 
        {
			break;
		}
	}
}

inline bool CShmQueue::IsEmpty() 
{
    lock();
    bool empty =  _queue.isEmpty();
    unlock();
    return empty;
}

inline unsigned int CShmQueue::count() 
{
    lock();
    unsigned int count = _queue.count();
    unlock();
    return count;
}
       

int main(int argc, char ** argv)
{
    if (argc < 2)
    {
        cout << "usage:\t" << argv[0] << " path_to_a_file" << endl;
        return 1;
    }
    CShmQueue queue;
    queue.init(argv[1], argv[1], 1000);    
    int i = 0;
    int _sendbuffer_size = 100;
    while (i++ < 100)
    {
        char *Buffer = "this is a test";
        queue.Enqueue(Buffer, strlen(Buffer));
    }
    sleep(1);
    i = 0;
    while (i++ < 100)
    {
        char *buffer = new char[_sendbuffer_size+3];
        memset(buffer, 0, _sendbuffer_size+3);
        unsigned len = _sendbuffer_size;
        queue.Dequeue(buffer, len);
        cout << buffer << endl;
        delete [] buffer;
    }
    return 0;
}


static void PyDelCShmQueue(void *ptr)
{
    CShmQueue* oldqueue = static_cast<CShmQueue*>(ptr);
    // cout << "is reclaimed" << endl;
    delete oldqueue;
    return;
}

PyObject* shmqueue_new_CShmQueue(PyObject *, PyObject* args)
{
    //动态创建一个新对象
    CShmQueue *newqueue = new CShmQueue();
    //把指针newnum包装成PyCObject对象并返回给解释器
    return PyCObject_FromVoidPtr(newqueue, PyDelCShmQueue);
}

static PyObject* shmqueue_init(PyObject* self, PyObject* args)
{
    PyObject* pyqueue = NULL;
    char* path_to_file;
    int queue_size;
    if (!PyArg_ParseTuple(args, "Osi", &pyqueue, &path_to_file, &queue_size))
        return NULL;
    // int key = CShmQueue::ftok(path_to_file);
    void * temp = PyCObject_AsVoidPtr(pyqueue);
    //把void指针转换为一个Numbers对象指针
    CShmQueue* queue = static_cast<CShmQueue*>(temp);
    //调用函数
    try
    {
        queue->init(path_to_file, path_to_file, queue_size);
    }
    catch (std::runtime_error &e)
    {
        char err_info[1024];
        snprintf(err_info, 1024, "Init Share memory queue failed  %s", e.what());
        PyErr_SetString(PyExc_RuntimeError, err_info);
        return NULL;
    }
    // cout << path_to_file << queue_size << endl;
    //return Py_BuildValue("b", 1);
    return Py_None;
}

static PyObject* shmqueue_enqueue(PyObject* self, PyObject* args)
{
    PyObject* pyqueue = NULL;
    char* info;
    if (!PyArg_ParseTuple(args, "Os", &pyqueue, &info))
        return NULL;
    void * temp = PyCObject_AsVoidPtr(pyqueue);
    //把void指针转换为一个Numbers对象指针
    CShmQueue* queue = static_cast<CShmQueue*>(temp);
    //调用函数
    try
    {
        queue->Enqueue(info, strlen(info));
    }
    catch (buffer_full &e)
    {
        char err_info[1024];
        snprintf(err_info, 1024, "The buffer queue is full  %s", e.what());
        PyErr_SetString(BufferFull, err_info);
        return NULL;
    }
    catch (std::runtime_error &e)
    {
        char err_info[1024];
        snprintf(err_info, 1024, "Exception in dequeue %s", e.what());
        PyErr_SetString(PyExc_RuntimeError, err_info);
        return NULL;
    }
    return Py_None;
}

static PyObject* shmqueue_dequeue(PyObject* self, PyObject* args)
{
    PyObject* pyqueue = NULL;
    unsigned int len;
    if (!PyArg_ParseTuple(args, "OI", &pyqueue, &len))
        return NULL;
    char* info = new char[len+3];
    memset(info, 0, len+3);
    void * temp = PyCObject_AsVoidPtr(pyqueue);
    //把void指针转换为一个Numbers对象指针
    CShmQueue* queue = static_cast<CShmQueue*>(temp);
    PyObject* retval = NULL; 
    try
    {
        if (queue->Dequeue(info, len))
            retval = (PyObject*)Py_BuildValue("s", info);
        else
            retval = (PyObject*)Py_BuildValue("");
    }
    catch (buffer_full &e)
    {
        char err_info[1024];
        snprintf(err_info, 1024, "The buffer queue is full  %s", e.what());
        PyErr_SetString(BufferFull, err_info);
        return NULL;
    }
    catch (std::runtime_error &e)
    {
        char err_info[1024];
        snprintf(err_info, 1024, "Exception in dequeue %s", e.what());
        PyErr_SetString(PyExc_RuntimeError, err_info);
        return NULL;
    }
    delete []info;
    return retval;
}

static PyObject* shmqueue_is_empty(PyObject* self, PyObject* args)
{
    PyObject* pyqueue = NULL;
    if (!PyArg_ParseTuple(args, "O", &pyqueue))
        return NULL;
    void * temp = PyCObject_AsVoidPtr(pyqueue);
    //把void指针转换为一个Numbers对象指针
    CShmQueue* queue = static_cast<CShmQueue*>(temp);
    bool is_empty = true;
    try
    {
        is_empty = queue->IsEmpty();
    }
    catch (std::runtime_error &e)
    {
        PyErr_SetString(PyExc_RuntimeError, "lock the queue failed");
        return NULL;
    }

    return PyBool_FromLong((long)is_empty);
}

//static PyObject* shmqueue_empty(PyObject* self, PyObject* args)
//{
//    PyObject* pyqueue = NULL;
//    if (!PyArg_ParseTuple(args, "O", &pyqueue))
//        return NULL;
//    void * temp = PyCObject_AsVoidPtr(pyqueue);
//    //把void指针转换为一个Numbers对象指针
//    CShmQueue* queue = static_cast<CShmQueue*>(temp);
//    return Py_BuildValue("b", 1);
//}

static PyObject* shmqueue_size(PyObject* self, PyObject* args)
{
    PyObject* pyqueue = NULL;
    if (!PyArg_ParseTuple(args, "O", &pyqueue))
        return NULL;
    void * temp = PyCObject_AsVoidPtr(pyqueue);
    //把void指针转换为一个Numbers对象指针
    CShmQueue* queue = static_cast<CShmQueue*>(temp);
    unsigned int size = 0;
    try
    {
        size = queue->count();
    }
    catch (std::runtime_error &e)
    {
        PyErr_SetString(PyExc_RuntimeError, "lock the queue failed");
        return NULL;
    }
    return Py_BuildValue("I", size);
}

static PyMethodDef shmqueueMethods[]= {
    {"CShmQueue",  shmqueue_new_CShmQueue, METH_NOARGS, "Create a ShmQueue Object"},
    {"init", shmqueue_init,METH_VARARGS,"Execute a shell command."},
    {"enqueue", shmqueue_enqueue,METH_VARARGS,"Execute a shell command."},
    {"dequeue", shmqueue_dequeue,METH_VARARGS,"Execute a shell command."},
    {"is_empty",shmqueue_is_empty,METH_VARARGS,"Execute a shell command."},
//    {"empty",shmqueue_empty,METH_VARARGS,"Execute a shell command."},
    {"size",shmqueue_size,METH_VARARGS,"Execute a shell command."},
    {NULL,NULL,0,NULL} 
};


PyMODINIT_FUNC initshmqueue(void)
{
    PyObject* module = Py_InitModule("shmqueue", shmqueueMethods);
    if (module == NULL)
    {
        return;
    }
    // create a new exception type which is inherited from exception
    BufferFull = PyErr_NewException("shmqueue.error", NULL, NULL);
    Py_INCREF(BufferFull);
    PyModule_AddObject(module, "error", BufferFull);
}
