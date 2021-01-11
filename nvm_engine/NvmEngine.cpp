#include "NvmEngine.hpp"
#include <libpmem.h>
#include <sys/mman.h>
#include <cassert>
#include <thread>
using namespace std;
using namespace __gnu_pbds;


#define CELL_SIZE(addr) (*((int*)(buff + addr + 20)))
#define BLOCK_SIZE(addr) (32 + CELL_SIZE(addr))
#define DATA_SZ(addr) (*((int*)(buff + addr + 16)))
#define NXT(addr) (*((ull*)(buff + addr + 24)))

Status DB::CreateOrOpen(const std::string& name, DB** dbptr, FILE* log_file) {
    if (log_file != NULL) {
        fprintf(log_file, "begin work\n");
    }
    return NvmEngine::CreateOrOpen(name, dbptr, log_file);
}

DB::~DB() {}
NvmEngine::NvmEngine(const std::string &name, FILE *log) {
	this->log = log;
	
	// 在AEP中申请的内存
	if ((buff = (char*)pmem_map_file(
		name.c_str(),
		AEP_SIZE * sizeof(char),
		PMEM_FILE_CREATE, 0666, 0,0)) == NULL) {
		fprintf(log, "Pmem map file failed[%s]\n", name.c_str());
		exit(0);
	}

	// 在DRAM内存中申请的内存
	memset(head, 0, sizeof(head));

	// 开始恢复持久化的数据
	ULL xxx = *((ULL*)buff);
	while (cursor <= xxx) {
		if (DATA_SZ(cursor) == 0)  // 跳过没有使用的
		{
			cursor += 32 + CELL_SIZE(cursor);
			continue;
		}
		uint32_t code = hash(buff + cursor, 16);

		ULL x = cursor; cursor += 32 + CELL_SIZE(cursor);
		NXT(x) = head[code];
		head[code] = x;
	}
}


Status NvmEngine::CreateOrOpen(const std::string& name, DB** dbptr, FILE *log) {
	*dbptr = new NvmEngine(name, log);
	return Ok;
}

// 读者
Status NvmEngine::Get(const Slice& key, std::string* value) {
	
	uint32_t code = hash(key.data(), 16);
	uint32_t slot = code / PER;
	while (spin_locks[slot].test_and_set(memory_order_acquire)) std::this_thread::yield();
	for (ULL i = head[code], p = 0; i; p = i, i = NXT(i)) {
		if (memcmp(key.data(), buff + i, 16) == 0) {
			value->assign(buff + i + 32, DATA_SZ(i));
			if (p) {
				NXT(p) = NXT(i);
				NXT(i) = head[code];
				head[code] = i;
			}
			spin_locks[slot].clear(memory_order_release);
			return Ok;
		}
	}
	spin_locks[slot].clear(memory_order_release);
	return NotFound;
}

// 写者
Status NvmEngine::Set(const Slice& key, const Slice& value) {
	//ULL zcc = tot.fetch_add(1);
	ul v_sz = value.size(), v_fix_sz = fixsize(v_sz);
	uint32_t code = hash(key.data(), 16);
	uint32_t slot = code / PER;
	while (spin_locks[slot].test_and_set(std::memory_order_acquire)) std::this_thread::yield();

	ull i, p = 0, i_nxt = 0;
	bool update = false;
	for (i = head[code]; i; p = i, i = NXT(i)) {
		if (memcmp(key.data(), buff + i, 16) == 0) {
			update = 1;
			i_nxt = NXT(i);
			break;
		}
	}

	ULL x = 0;

	ull addr = 0;
	if (freenodes[slot&QUEUE_MASK].pop(addr))
	{
		if (addr && CELL_SIZE(addr) >= v_sz) {
			x = addr;
		}
		else {
			while (!freenodes[slot&QUEUE_MASK].push(addr)) this_thread::yield();
		}
	}


	// 找到了一个 存储块，其大小 cell_size 不能修改
	if (x)
	{
		DATA_SZ(x) = v_sz;
		memcpy(buff + x + 32, value.data(), v_sz);
		memcpy(buff + x, key.data(), 16);
		NXT(x) = head[code];
		head[code] = x;
	}
	else
	{
		 {
			x = cursor.fetch_add(32 + v_fix_sz);
			*((ull*)buff) = x;
			pmem_flush(buff, 8);
			pmem_drain();
		}
		
		CELL_SIZE(x) = v_fix_sz;
		memcpy(buff + x + 32, value.data(), v_sz);
		pmem_flush(buff + x + 16, 16 + v_sz);

		memcpy(buff + x, key.data(), 16);
		pmem_flush(buff + x, 16);

		pmem_memcpy_persist(buff + x + 24, &head[code], 8);
		pmem_memcpy_persist(buff + x + 16, &v_sz, 4);
		head[code] = x;

	}

	if (update)
	{
		//新的数据使用节点x已经插入在表头， 还需要删除空闲结点 i，
		if (p)
		{
			NXT(p) = i_nxt;
		}
		else {
			NXT(x) = i_nxt;
		}
		DATA_SZ(i) = 0;
		NXT(i) = 0;
		while (!freenodes[slot&QUEUE_MASK].push(i)) this_thread::yield();
	}

	spin_locks[slot].clear(std::memory_order_release);

	return Ok;
}

NvmEngine::~NvmEngine() { }


