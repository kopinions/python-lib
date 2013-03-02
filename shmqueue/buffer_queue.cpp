#include "buffer_queue.h"
#include <stdexcept>
#include <iostream>
#include <assert.h>
#include <errno.h> 
#include <unistd.h>
using namespace std;


void CBufferQueue::attach(char* pBuf, unsigned long iBufSize) throw (runtime_error)
{
	if((unsigned long)pBuf % sizeof(unsigned long) != 0) { // 保护
		throw runtime_error("CBufferQueue::create fail:pBuf must % sizeof(unsigned long) == 0");
	}
	if(iBufSize <= sizeof(Header)+sizeof(unsigned long)+ReserveLen) {
		throw runtime_error("CBufferQueue::create fail:pBuf must % sizeof(unsigned long) == 0");
	}

	_header = (Header *)pBuf;
	_data = pBuf+sizeof(Header);

	if(_header->iBufSize != iBufSize - sizeof(Header))
		throw runtime_error("CBufferQueue::attach fail: iBufSize != iBufSize - sizeof(Header);");
	if(_header->iReserveLen != ReserveLen)
		throw runtime_error("CBufferQueue::attach fail: iReserveLen != ReserveLen");
	if(_header->iBegin >= _header->iBufSize)
		throw runtime_error("CBufferQueue::attach fail: iBegin > iBufSize - sizeof(Header);");
	if(_header->iEnd > iBufSize - sizeof(Header))
		throw runtime_error("CBufferQueue::attach fail: iEnd > iBufSize - sizeof(Header);");
}

void CBufferQueue::create(char* pBuf, unsigned long iBufSize) throw (runtime_error)// attach and init 
{
	if((unsigned long)pBuf % sizeof(unsigned long) != 0) { // 保护
		throw runtime_error("CBufferQueue::create fail:pBuf must % sizeof(unsigned long) == 0");
	}
	if(iBufSize <= sizeof(Header)+sizeof(unsigned long)+ReserveLen) {
		throw runtime_error("CBufferQueue::create fail:pBuf must % sizeof(unsigned long) == 0");
	}

	_header = (Header *)pBuf;
	_data = pBuf+sizeof(Header);

	_header->iBufSize = iBufSize - sizeof(Header);
	_header->iReserveLen = ReserveLen;
	_header->iBegin = 0;
	_header->iEnd = 0;
    _header->iCount = 0; 
}

bool CBufferQueue::dequeue(char *buffer,unsigned & buffersize) throw(buffer_full)
{
	if(isEmpty()) {
		return false;
	}

    //无论成功与否，消息个数都减1
    if(_header->iCount)
    {
       _header->iCount--; 
    }
    
	if(_header->iEnd > _header->iBegin) {
		assert(_header->iBegin+sizeof(unsigned long) < _header->iEnd);
		unsigned long len = GetLen(_data+_header->iBegin);
		assert(_header->iBegin+sizeof(unsigned long)+len <= _header->iEnd);
		if(len > buffersize) {
			_header->iBegin += len+sizeof(unsigned long);
			throw buffer_full("CBufferQueue::dequeue data is too long to store in the buffer");
		}
		buffersize = len;
		memcpy(buffer,_data+_header->iBegin+sizeof(unsigned long),len);
		_header->iBegin += len+sizeof(unsigned long);
	} else {
		// 被分段
		assert(_header->iEnd+ReserveLen <= _header->iBegin);
		unsigned long len = 0;
		unsigned long new_begin = 0;
		char *data_from = NULL;
		char *data_to = NULL;
		assert(_header->iBegin+1 <= _header->iBufSize);
		// 长度字段也被分段
		if(_header->iBegin+sizeof(unsigned long) > _header->iBufSize) { 
			char tmp[16];
			memcpy(tmp,_data+_header->iBegin,_header->iBufSize-_header->iBegin);
			memcpy(tmp+_header->iBufSize-_header->iBegin,_data,_header->iBegin+sizeof(unsigned long)-_header->iBufSize);
			len = GetLen(tmp);
			data_from = _data+(_header->iBegin+sizeof(unsigned long)-_header->iBufSize); //
			new_begin = _header->iBegin+sizeof(unsigned long)-_header->iBufSize+len;
			assert(new_begin <= _header->iEnd);
		} else {
			len = GetLen(_data+_header->iBegin);
			data_from = _data+_header->iBegin+sizeof(unsigned long);
			if(data_from == _data+_header->iBufSize) data_from = _data;
			if(_header->iBegin+sizeof(unsigned long)+len < _header->iBufSize) { 
				new_begin = _header->iBegin+sizeof(unsigned long)+len;
			} else { // 数据被分段
				new_begin = _header->iBegin+sizeof(unsigned long)+len-_header->iBufSize;
				assert(new_begin <= _header->iEnd);
			}
		}
		data_to = _data+new_begin;
		_header->iBegin = new_begin;

		if(len > buffersize) {
			throw buffer_full("CBufferQueue::dequeue data is too long to store in the buffer");
		}
		buffersize = len;
		if(data_to > data_from) {
			assert(data_to - data_from == (long)len);
			memcpy(buffer,data_from,len);
		} else {
			memcpy(buffer,data_from,_data-data_from+_header->iBufSize);
			memcpy(buffer+(_data-data_from+_header->iBufSize),_data,data_to-_data);
			assert(_header->iBufSize-(data_from-data_to)== len);
		}
	}

	return true;
}

bool CBufferQueue::peek(char *buffer,unsigned & buffersize) throw(buffer_full)
{
	if(isEmpty()) {
		return false;
	}

	if(_header->iEnd > _header->iBegin) {
		assert(_header->iBegin+sizeof(unsigned long) < _header->iEnd);
		unsigned long len = GetLen(_data+_header->iBegin);
		assert(_header->iBegin+sizeof(unsigned long)+len <= _header->iEnd);
		if(len > buffersize) {
			//_header->iBegin += len+sizeof(unsigned long);
			throw buffer_full("CBufferQueue::peek data is too long to store in the buffer");
		}
		buffersize = len;
		memcpy(buffer,_data+_header->iBegin+sizeof(unsigned long),len);
		//_header->iBegin += len+sizeof(unsigned long);
	} else {
		// 被分段
		assert(_header->iEnd+ReserveLen <= _header->iBegin);
		unsigned long len = 0;
		unsigned long new_begin = 0;
		char *data_from = NULL;
		char *data_to = NULL;
		assert(_header->iBegin+1 <= _header->iBufSize);
		// 长度字段也被分段
		if(_header->iBegin+sizeof(unsigned long) > _header->iBufSize) { 
			char tmp[16];
			memcpy(tmp,_data+_header->iBegin,_header->iBufSize-_header->iBegin);
			memcpy(tmp+_header->iBufSize-_header->iBegin,_data,_header->iBegin+sizeof(unsigned long)-_header->iBufSize);
			len = GetLen(tmp);
			data_from = _data+(_header->iBegin+sizeof(unsigned long)-_header->iBufSize); //
			new_begin = _header->iBegin+sizeof(unsigned long)-_header->iBufSize+len;
			assert(new_begin <= _header->iEnd);
		} else {
			len = GetLen(_data+_header->iBegin);
			data_from = _data+_header->iBegin+sizeof(unsigned long);
			if(data_from == _data+_header->iBufSize) data_from = _data;
			if(_header->iBegin+sizeof(unsigned long)+len < _header->iBufSize) { 
				new_begin = _header->iBegin+sizeof(unsigned long)+len;
			} else { // 数据被分段
				new_begin = _header->iBegin+sizeof(unsigned long)+len-_header->iBufSize;
				assert(new_begin <= _header->iEnd);
			}
		}
		data_to = _data+new_begin;
		//_header->iBegin = new_begin;

		if(len > buffersize) {
			throw buffer_full("CBufferQueue::peek data is too long to store in the buffer");
		}
		buffersize = len;
		if(data_to > data_from) {
			assert(data_to - data_from == (long)len);
			memcpy(buffer,data_from,len);
		} else {
			memcpy(buffer,data_from,_data-data_from+_header->iBufSize);
			memcpy(buffer+(_data-data_from+_header->iBufSize),_data,data_to-_data);
			assert(_header->iBufSize-(data_from-data_to)== len);
		}
	}

	return true;
}

bool CBufferQueue::dequeue(char *buffer1,unsigned & buffersize1,char *buffer2,unsigned & buffersize2) throw(buffer_full)
{
	unsigned long buffersize = buffersize1+buffersize2;

	if(isEmpty()) {
		return false;
	}

    if(_header->iCount)
    {
       _header->iCount--; 
    }
       
	if(_header->iEnd > _header->iBegin) {
		assert(_header->iBegin+sizeof(unsigned long) < _header->iEnd);
		unsigned long len = GetLen(_data+_header->iBegin);
		assert(_header->iBegin+sizeof(unsigned long)+len <= _header->iEnd);
		if(len > buffersize) {
			_header->iBegin += len+sizeof(unsigned long);
			cerr << "CBufferQueue::dequeue: len=" << len << " bufsize=" << buffersize << endl;
			throw buffer_full("CBufferQueue::dequeue data is too long tostore in the buffer");
		}
		if(buffersize1 > len) {
			buffersize1 = len;
			buffersize2 = 0;
			memcpy(buffer1,_data+_header->iBegin+sizeof(unsigned long),len);
		} else {
			buffersize2 = len-buffersize1;
			memcpy(buffer1,_data+_header->iBegin+sizeof(unsigned long),buffersize1);
			memcpy(buffer2,_data+_header->iBegin+sizeof(unsigned long)+buffersize1,buffersize2);
		}
		_header->iBegin += len+sizeof(unsigned long);
	} else {
		// 被分段
		assert(_header->iEnd+ReserveLen <= _header->iBegin);
		unsigned long len = 0;
		unsigned long new_begin = 0;
		char *data_from = NULL;
		char *data_to = NULL;
		assert(_header->iBegin+1 <= _header->iBufSize);
		// 长度字段也被分段
		if(_header->iBegin+sizeof(unsigned long) > _header->iBufSize) { 
			char tmp[16];
			memcpy(tmp,_data+_header->iBegin,_header->iBufSize-_header->iBegin);
			memcpy(tmp+_header->iBufSize-_header->iBegin,_data,_header->iBegin+sizeof(unsigned long)-_header->iBufSize);
			len = GetLen(tmp);
			data_from = _data+(_header->iBegin+sizeof(unsigned long)-_header->iBufSize); //
			new_begin = (_header->iBegin+sizeof(unsigned long)-_header->iBufSize)+len;
			assert(new_begin <= _header->iEnd);
		} else {
			len = GetLen(_data+_header->iBegin);
			data_from = _data+_header->iBegin+sizeof(unsigned long);
			if(data_from == _data+_header->iBufSize) data_from = _data;
			if(_header->iBegin+sizeof(unsigned long)+len < _header->iBufSize) { 
				new_begin = _header->iBegin+sizeof(unsigned long)+len;
			} else { // 数据被分段
				new_begin = _header->iBegin+sizeof(unsigned long)+len-_header->iBufSize;
				assert(new_begin <= _header->iEnd);
			}
		}
		data_to = _data+new_begin;
		_header->iBegin = new_begin;

		if(len > buffersize) {
			throw buffer_full("CBufferQueue::dequeue data is too long to store in the buffer");
		}
		buffersize = len;
		if(data_to > data_from) {
			assert(data_to - data_from == (long)len);
			if(buffersize1 > len) {
				buffersize1 = len;
				buffersize2 = 0;
				memcpy(buffer1,data_from,len);
			} else {
				buffersize2 = len-buffersize1;
				memcpy(buffer1,data_from,buffersize1);
				memcpy(buffer2,data_from+buffersize1,buffersize2);
			}
		} else {
			assert(_header->iBufSize-(data_from-data_to)== len);
			if(buffersize1>len) {
				buffersize1 = len;
				buffersize2 = 0;
				memcpy(buffer1,data_from,_data+_header->iBufSize-data_from);
				memcpy(buffer1+(_data+_header->iBufSize-data_from),_data,data_to-_data);
			} else if(buffersize1>_data-data_from+_header->iBufSize) { //buffer1被分段
				buffersize2 = len-buffersize1;
				memcpy(buffer1,data_from,_data+_header->iBufSize-data_from);
				memcpy(buffer1+(_data+_header->iBufSize-data_from),_data,buffersize1-(_data+_header->iBufSize-data_from));
				memcpy(buffer2,data_from+buffersize1-_header->iBufSize,buffersize2);
			} else { //buffer2被分段
				buffersize2 = len-buffersize1;
				memcpy(buffer1,data_from,buffersize1);
				memcpy(buffer2,data_from+buffersize1,_data+_header->iBufSize-data_from-buffersize1);
				memcpy(buffer2+(_data+_header->iBufSize-data_from-buffersize1),_data,len-(_data-data_from+_header->iBufSize));
			}
		}
	}

	return true;
}
bool CBufferQueue::peek(char *buffer1,unsigned & buffersize1,char *buffer2,unsigned & buffersize2) throw(buffer_full)
{
	unsigned long buffersize = buffersize1+buffersize2;

	if(isEmpty()) {
		return false;
	}

	if(_header->iEnd > _header->iBegin) {
		assert(_header->iBegin+sizeof(unsigned long) < _header->iEnd);
		unsigned long len = GetLen(_data+_header->iBegin);
		assert(_header->iBegin+sizeof(unsigned long)+len <= _header->iEnd);
		if(len > buffersize) {
			_header->iBegin += len+sizeof(unsigned long);
			cerr << "CBufferQueue::dequeue: len=" << len << " bufsize=" << buffersize << endl;
			throw buffer_full("CBufferQueue::dequeue data is too long tostore in the buffer");
		}
		if(buffersize1 > len) {
			buffersize1 = len;
			buffersize2 = 0;
			memcpy(buffer1,_data+_header->iBegin+sizeof(unsigned long),len);
		} else {
			buffersize2 = len-buffersize1;
			memcpy(buffer1,_data+_header->iBegin+sizeof(unsigned long),buffersize1);
			memcpy(buffer2,_data+_header->iBegin+sizeof(unsigned long)+buffersize1,buffersize2);
		}
		//_header->iBegin += len+sizeof(unsigned long);
	} else {
		// 被分段
		assert(_header->iEnd+ReserveLen <= _header->iBegin);
		unsigned long len = 0;
		unsigned long new_begin = 0;
		char *data_from = NULL;
		char *data_to = NULL;
		assert(_header->iBegin+1 <= _header->iBufSize);
		// 长度字段也被分段
		if(_header->iBegin+sizeof(unsigned long) > _header->iBufSize) { 
			char tmp[16];
			memcpy(tmp,_data+_header->iBegin,_header->iBufSize-_header->iBegin);
			memcpy(tmp+_header->iBufSize-_header->iBegin,_data,_header->iBegin+sizeof(unsigned long)-_header->iBufSize);
			len = GetLen(tmp);
			data_from = _data+(_header->iBegin+sizeof(unsigned long)-_header->iBufSize); //
			new_begin = (_header->iBegin+sizeof(unsigned long)-_header->iBufSize)+len;
			assert(new_begin <= _header->iEnd);
		} else {
			len = GetLen(_data+_header->iBegin);
			data_from = _data+_header->iBegin+sizeof(unsigned long);
			if(data_from == _data+_header->iBufSize) data_from = _data;
			if(_header->iBegin+sizeof(unsigned long)+len < _header->iBufSize) { 
				new_begin = _header->iBegin+sizeof(unsigned long)+len;
			} else { // 数据被分段
				new_begin = _header->iBegin+sizeof(unsigned long)+len-_header->iBufSize;
				assert(new_begin <= _header->iEnd);
			}
		}
		data_to = _data+new_begin;
		//_header->iBegin = new_begin;

		if(len > buffersize) {
			throw buffer_full("CBufferQueue::dequeue data is too long to store in the buffer");
		}
		buffersize = len;
		if(data_to > data_from) {
			assert(data_to - data_from == (long)len);
			if(buffersize1 > len) {
				buffersize1 = len;
				buffersize2 = 0;
				memcpy(buffer1,data_from,len);
			} else {
				buffersize2 = len-buffersize1;
				memcpy(buffer1,data_from,buffersize1);
				memcpy(buffer2,data_from+buffersize1,buffersize2);
			}
		} else {
			assert(_header->iBufSize-(data_from-data_to)== len);
			if(buffersize1>len) {
				buffersize1 = len;
				buffersize2 = 0;
				memcpy(buffer1,data_from,_data+_header->iBufSize-data_from);
				memcpy(buffer1+(_data+_header->iBufSize-data_from),_data,data_to-_data);
			} else if(buffersize1>_data-data_from+_header->iBufSize) { //buffer1被分段
				buffersize2 = len-buffersize1;
				memcpy(buffer1,data_from,_data+_header->iBufSize-data_from);
				memcpy(buffer1+(_data+_header->iBufSize-data_from),_data,buffersize1-(_data+_header->iBufSize-data_from));
				memcpy(buffer2,data_from+buffersize1-_header->iBufSize,buffersize2);
			} else { //buffer2被分段
				buffersize2 = len-buffersize1;
				memcpy(buffer1,data_from,buffersize1);
				memcpy(buffer2,data_from+buffersize1,_data+_header->iBufSize-data_from-buffersize1);
				memcpy(buffer2+(_data+_header->iBufSize-data_from-buffersize1),_data,len-(_data-data_from+_header->iBufSize));
			}
		}
	}

	return true;
}
void CBufferQueue::enqueue(const char *buffer,unsigned len) throw(buffer_full)
{
	if(len == 0) return;
	if(isFull(len)) throw buffer_full("CBufferQueue::enqueue full");

	// 长度字段被分段
	if(_header->iEnd+sizeof(unsigned long) > _header->iBufSize) {
		char tmp[16]; SetLen(tmp,len);
		memcpy(_data+_header->iEnd,tmp,_header->iBufSize-_header->iEnd);
		memcpy(_data,tmp+_header->iBufSize-_header->iEnd,_header->iEnd+sizeof(unsigned long)-_header->iBufSize);
		memcpy(_data+_header->iEnd+sizeof(unsigned long)-_header->iBufSize,buffer,len);
		_header->iEnd = len+_header->iEnd+sizeof(unsigned long)-_header->iBufSize;
		assert(_header->iEnd+ReserveLen <= _header->iBegin);
	} 
	// 数据被分段
	else if(_header->iEnd+sizeof(unsigned long)+len > _header->iBufSize){
		SetLen(_data+_header->iEnd,len);
		memcpy(_data+_header->iEnd+sizeof(unsigned long),buffer,_header->iBufSize-_header->iEnd-sizeof(unsigned long));
		memcpy(_data,buffer+_header->iBufSize-_header->iEnd-sizeof(unsigned long),len-(_header->iBufSize-_header->iEnd-sizeof(unsigned long)));
		_header->iEnd = len-(_header->iBufSize-_header->iEnd-sizeof(unsigned long));
		assert(_header->iEnd+ReserveLen <= _header->iBegin);
	} else {
		SetLen(_data+_header->iEnd,len);
		memcpy(_data+_header->iEnd+sizeof(unsigned long),buffer,len);
		_header->iEnd = (_header->iEnd+sizeof(unsigned long)+len)%_header->iBufSize;
	}

       _header->iCount++;
}

void CBufferQueue::enqueue(const char *buffer1,unsigned len1,const char *buffer2,unsigned len2) throw(buffer_full)
{
	unsigned long len = len1+len2;
	assert(_header);
	assert(_data);
	if(len == 0) return;
	if(isFull(len)) throw buffer_full("CBufferQueue::enqueue full");

	// 长度字段被分段
	if(_header->iEnd+sizeof(unsigned long) > _header->iBufSize) {
		char tmp[16]; SetLen(tmp,len);
		memcpy(_data+_header->iEnd,tmp,_header->iBufSize-_header->iEnd);
		memcpy(_data,tmp+_header->iBufSize-_header->iEnd,_header->iEnd+sizeof(unsigned long)-_header->iBufSize);
		memcpy(_data+_header->iEnd+sizeof(unsigned long)-_header->iBufSize,buffer1,len1);
		memcpy(_data+_header->iEnd+sizeof(unsigned long)-_header->iBufSize+len1,buffer2,len2);
		_header->iEnd = len+_header->iEnd+sizeof(unsigned long)-_header->iBufSize;
		assert(_header->iEnd+ReserveLen <= _header->iBegin);
	} 
	// 数据被分段
	else if(_header->iEnd+sizeof(unsigned long)+len > _header->iBufSize){
		SetLen(_data+_header->iEnd,len);
		if(_header->iEnd+sizeof(unsigned long)+len1>_header->iBufSize) { //buffer1被分段
			memcpy(_data+_header->iEnd+sizeof(unsigned long),buffer1,_header->iBufSize-_header->iEnd-sizeof(unsigned long));
			memcpy(_data,buffer1+_header->iBufSize-_header->iEnd-sizeof(unsigned long),len1-(_header->iBufSize-_header->iEnd-sizeof(unsigned long)));
			memcpy(_data+len1-(_header->iBufSize-_header->iEnd-sizeof(unsigned long)),buffer2,len2);
		} else { //buffer2被分段
			memcpy(_data+_header->iEnd+sizeof(unsigned long),buffer1,len1);
			memcpy(_data+_header->iEnd+sizeof(unsigned long)+len1,buffer2,_header->iBufSize-_header->iEnd-sizeof(unsigned long)-len1);
			memcpy(_data,buffer2+_header->iBufSize-_header->iEnd-sizeof(unsigned long)-len1,len2-(_header->iBufSize-_header->iEnd-sizeof(unsigned long)-len1));
		}
		_header->iEnd = len-(_header->iBufSize-_header->iEnd-sizeof(unsigned long));
		assert(_header->iEnd+ReserveLen <= _header->iBegin);
	} else {
		SetLen(_data+_header->iEnd,len);
		memcpy(_data+_header->iEnd+sizeof(unsigned long),buffer1,len1);
		memcpy(_data+_header->iEnd+sizeof(unsigned long)+len1,buffer2,len2);
		_header->iEnd = (_header->iEnd+sizeof(unsigned long)+len)%_header->iBufSize;
	}

       _header->iCount++;
}

bool CBufferQueue::isFull(unsigned long len) const
{
	if(len==0) return false;

	if(_header->iEnd == _header->iBegin) {
		if(len+sizeof(unsigned long)+ReserveLen > _header->iBufSize) return true;
		return false;
	}

	if(_header->iEnd > _header->iBegin) {
		assert(_header->iBegin+sizeof(unsigned long) < _header->iEnd);
		return _header->iBufSize - _header->iEnd + _header->iBegin < sizeof(unsigned long)+len+ReserveLen;
	}
/*
	if(_header->iEnd+ReserveLen > _header->iBegin) {
		cerr << "_header:" 
			<< " iBufSize:" << _header->iBufSize
			<< " iBegin:" << _header->iBegin 
			<< " iEnd:" << _header->iEnd 
			<< endl;
	}
*/	assert(_header->iEnd+ReserveLen <= _header->iBegin);
	return (_header->iBegin - _header->iEnd < sizeof(unsigned long)+len+ReserveLen);
}

unsigned int CBufferQueue::count() const
{
    return _header ? _header->iCount : 0;
}



