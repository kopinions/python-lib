
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
     * @param pBuf 		�ڴ���ʼ��ַ
     * @param iBufSize 	�ڴ���ܵĴ�С
     * @throw runtime_error: 
     */
    void attach(char* pBuf, unsigned long iBufSize) throw (std::runtime_error);
    void create(char* pBuf, unsigned long iBufSize) throw (std::runtime_error); // attach and init 

    /**
     * ������
     * @param buffer 		bufferָ��
     * @param buffersize 	buffer��󳤶ȣ�����ɹ�ȡ������buffersizeΪȡ�����ݵĴ�С
     * @return 			true-ȡ���ݳɹ� false-����Ϊ��
     * @throw buffer_full	��������ȡ��������> buffersizeʱ�׳�����ʱ�Ĵ����������Ȼ��Ӷ��������
     */
    bool dequeue(char *buffer,unsigned & buffersize) throw(buffer_full);
    /**
     * ���������Ӧ�ó���: 
     * ��queue�з�һ���̶���С�����ݿ��һ���䳤�����ݿ�,����һ�β����ȡ��
     */
    bool dequeue(char *buffer1,unsigned & buffersize1,char *buffer2,unsigned & buffersize2) throw(buffer_full);

    /**
     * peek���Ὣ���ݴ�queue���
     */
    bool peek(char *buffer,unsigned & buffersize) throw(buffer_full);
    bool peek(char *buffer1,unsigned & buffersize1,char *buffer2,unsigned & buffersize2) throw(buffer_full);

    /**
     * �����
     * @param buffer 		
     * @param len		���ݳ��� 
     * @throw buffer_full	��������ȡ��������> buffersizeʱ�׳�����ʱ�Ĵ����������Ȼ��Ӷ��������
     */
    void enqueue(const char *buffer,unsigned len) throw(buffer_full);
    void enqueue(const char *buffer1,unsigned len1,const char *buffer2,unsigned len2) throw(buffer_full);

    bool isEmpty() const {return _header->iBegin == _header->iEnd;}
    bool isFull(unsigned long len) const;

      // ������Ϣ����.
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
