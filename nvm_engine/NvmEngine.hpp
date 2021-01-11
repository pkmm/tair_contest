#ifndef TAIR_CONTEST_KV_CONTEST_NVM_ENGINE_H_
#define TAIR_CONTEST_KV_CONTEST_NVM_ENGINE_H_

#include "include/db.hpp"
#include <cstring>
#include <stdlib.h>
#include <cstdio>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <stdint.h>
#include <list>
#include <queue>
#include <ctime>
#include <functional>
#include<ext/pb_ds/priority_queue.hpp>
#include <boost/lockfree/queue.hpp>

#define FX 0
constexpr auto A = 128;// 256;
constexpr auto B = 256;// 300;
constexpr auto C = 512;
constexpr auto D = 1024;

typedef uint64_t ULL;
typedef uint64_t ull;
typedef uint32_t ul;

const ULL K = 1024;
const ULL M = ULL(1024) * K;
const ULL G = ULL(1024) * M;


const uint32_t PER = 1; // 每一个锁管理PER个数据ID
const uint32_t HEAD_SZIE = 268435456; //  16777216; // min(2^n) > 1e7; [1e7, 16777216] 【1e8, 134217728】【2e8, 268435456】【8e8, 1073741824】
const uint32_t LOCK_SIZE = HEAD_SZIE / PER + 10; // 锁的个数

const uint32_t MOD = HEAD_SZIE - 1;
const ul FREE_QUEUE_N = 16;
const ul QUEUE_MASK = FREE_QUEUE_N - 1;

const ULL AEP_SIZE = 64 * G;

class NvmEngine : DB {
public:
	/**
	 * @param
	 * name: file in AEP(exist)
	 * dbptr: pointer of db object
	 *
	 */
	static Status CreateOrOpen(const std::string& name, DB** dbptr, FILE *log);
	Status Get(const Slice& key, std::string* value) override;
	Status Set(const Slice& key, const Slice& value) override;
	~NvmEngine();
	NvmEngine(const std::string&name, FILE*);
private:
	FILE *log;

	char *buff;

	// 使用atomic实现自旋锁
	std::atomic_flag spin_locks[LOCK_SIZE];
	std::atomic_flag cursor_lock = ATOMIC_FLAG_INIT;
	std::atomic_flag list_lock = ATOMIC_FLAG_INIT;
	ULL head[HEAD_SZIE];
	std::atomic<ull> cursor = { 8 }; // 内存游标
	std::atomic<ULL> tot = { 0 };
	std::mutex q_lock, c_lock;
	struct Node
	{
		ULL addr;
		ul sz;
		bool operator<(const Node& oth) const
		{
			return addr > oth.addr;
			//return sz > oth.sz || sz == oth.sz && addr > oth.addr; 
		}
	};
	//__gnu_pbds::priority_queue<Node, std::less<Node>, __gnu_pbds::pairing_heap_tag> freenodes;
	//std::queue<Node> freenodes;

	boost::lockfree::queue<ull> freenodes[FREE_QUEUE_N];

	ul hash(const void *key, int len) {
		uint32_t seed = 5381;
		const uint32_t m = 0x5bd1e995;
		const int r = 24;
		uint32_t h = seed ^ len;
		const unsigned char *data = (const unsigned char *)key;
		while (len >= 4) {
			uint32_t k = *(uint32_t*)data;
			k *= m;
			k ^= k >> r;
			k *= m;
			h *= m;
			h ^= k;
			data += 4;
			len -= 4;
		}
		switch (len) {
		case 3: h ^= data[2] << 16;
		case 2: h ^= data[1] << 8;
		case 1: h ^= data[0]; h *= m;
		};
		h ^= h >> 13;
		h *= m;
		h ^= h >> 15;
		return ((ul)h) & 0x7fffffff & MOD;

	}

	int fixsize(int size)
	{
		if (size <= A) return A;
		return size;
	/*	else if (size <= B) return B;
		else if (size <= C) return C;
		else  return D;*/
	}
};


#endif
