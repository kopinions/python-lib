
#ifndef __BUFFER_QUEUE_H__
#define __BUFFER_QUEUE_H__

#include <vector>
#include <sys/time.h>
#include <stdexcept>
#include <cstring>

struct buffer_full: public std::runtime_error
{
    buffer_full(const std::string& s):std::runtime_error(s){}
};

class CBufferQueue
{
public:
    CBufferQueue() {_header = NULL; _data = NULL; }
    ~CBufferQueue() {_header = NULL; _data = NULL;}

    /**
     * 
     * @param pBuf 		内存起始地址
     * @param iBufSize 	内存块总的大小
     * @throw runtime_error: 
     */
    void attach(char* pBuf, unsigned long iBufSize) throw (std::runtime_error);
    void create(char* pBuf, unsigned long iBufSize) throw (std::runtime_error); // attach and init 

    /**
     * 出队列
     * @param buffer 		buffer指针
     * @param buffersize 	buffer最大长度，如果成功取出数据buffersize为取出数据的大小
     * @return 			true-取数据成功 false-队列为空
     * @throw buffer_full	当队列中取到的数据> buffersize时抛出，此时的处理该数据仍然会从队列中清除
     */
    bool dequeue(char *buffer,unsigned & buffersize) throw(buffer_full);
    /**
     * 针对这样的应用场景: 
     * 在queue中放一个固定大小的数据块和一个变长的数据块,可以一次插入或取出
     */
    bool dequeue(char *buffer1,unsigned & buffersize1,char *buffer2,unsigned & buffersize2) throw(buffer_full);

    /**
     * peek不会将数据从queue清除
     */
    bool peek(char *buffer,unsigned & buffersize) throw(buffer_full);
    bool peek(char *buffer1,unsigned & buffersize1,char *buffer2,unsigned & buffersize2) throw(buffer_full);

    /**
     * 入队列
     * @param buffer 		
     * @param len		数据长度 
     * @throw buffer_full	当队列中取道的数据> buffersize时抛出，此时的处理该数据仍然会从队列中清除
     */
    void enqueue(const char *buffer,unsigned len) throw(buffer_full);
    void enqueue(const char *buffer1,unsigned len1,const char *buffer2,unsigned len2) throw(buffer_full);

    bool isEmpty() const {return _header->iBegin == _header->iEnd;}
    bool isFull(unsigned long len) const;

      // 返回消息个数.
      unsigned int count() const;
private:
    unsigned long GetLen(char *buf) {unsigned long u; memcpy((void *)&u,buf,sizeof(unsigned long)); return u;}
    void SetLen(char *buf,unsigned long u) {memcpy(buf,(void *)&u,sizeof(unsigned long));}

private:
    const static unsigned long ReserveLen = 8;
    typedef struct Header
    {
        unsigned long iBufSize;
        unsigned long iReserveLen; // must be 8
        unsigned long iBegin;
        unsigned long iEnd;
             unsigned int iCount; // msg #
    } Header;

    Header *_header;
    char *_data;

};


#endif
