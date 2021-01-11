# tair contest
阿里云的第二届数据库大赛—Tair性能挑战大赛，决赛取得第27(共2170队伍)名



本小白第一次参加这样的比赛，也是第一次使用c++写多线程的代码和可持久化的代码，很多东西都不知道。

比赛过程中学习了 `多线程` `自旋锁` `无锁的队列` `原子操作CAS` `copy on write机制` `CPU cache line`  `原子操作`  `事务ACID` `硬件指令CRC32`等知识点

c++写的不多，代码实现比较简陋、幼稚。数据结构主要是使用hash表，使用自旋锁和无锁的队列实现hash表上的分段锁，降低锁的争用。

这个仓库主要是为了记录下自己的学习经历，欢迎交批评指正

学习过程中找到的一个不错的关于RCU的一个视频（在youtube上可能需要科学上网），[RCU介绍](https://www.youtube.com/watch?reload=9&v=rxQ5K9lo034)



下面是赛题的介绍

# 赛题介绍

## 竞赛题目

Tair是阿里云自研的云原生内存数据库，接口兼容开源Redis/Memcache。在阿里巴巴集团内和阿里云上提供了缓存服务和高性能存储服务，追求极致的性能和稳定性。全新英特尔® 傲腾™ 数据中心级持久内存重新定义了传统的架构在内存密集型工作模式，具有突破性的性能水平，同时具有持久化能力。Aliyun弹性计算服务首次（全球首家）在神龙裸金属服务器上引入傲腾持久内存，撘配阿里云官方提供的Linux操作系统镜像Aliyun Linux，深度优化完善支持，为客户提供安全、稳定、高性能的体验。本题结合Tair基于神龙非易失性内存增强型裸金属实例和Aliyun Linux操作系统，探索新介质和新软件系统上极致的持久化和性能。参赛者在充分认知Aep硬件特质特性的情况下设计最优的数据结构。

## 赛题说明

热点是阿里巴巴双十一洪峰流量下最重要的一个问题，双十一淘宝首页上的一件热门商品的访问TPS在千万级别。这样的热点Key，在分布式的场景下也只会路由到其中的一台服务器上。解决这个问题的方法有Tair目前提供的热点散列机制，同时在单节点能力上需要做更多的增强。

本题设计一个基于傲腾持久化内存(Aep)的KeyValue单机引擎，支持Set和Get的数据接口，同时对于热点访问具有良好的性能。

## 评测逻辑

评测程序会调用参赛选手的接口，启动16个线程进行写入和读取数据，最终统计读写完指定数目所用的时间，按照使用时间的从低到高排名。

评测分为两个阶段: 正确性评测和(初/复赛)性能评测

1）程序正确性评测，验证数据读写的正确性（复赛会增加持久化能力的验证），这部分的耗时不计入运行时间的统计。

2）初赛性能评测

引擎使用的内存和持久化内存限制在 4G Dram和 74G Aep。

每个线程分别写入约48M个Key大小为16Bytes，Value大小为80Bytes的KV对象，接着以95：5的读写比例访问调用24M次。其中95%的读访问具有热点的特征，大部分的读访问集中在少量的Key上面。

3）复赛性能评测

引擎使用的内存和持久化内存限制在16G Dram和 128G Aep，复赛要求数据具有持久化和可恢复(Crash-Recover)的能力，确保在接口调用后，即使掉电的时候能保证数据的完整性和正确恢复。

运行流程和初赛一致，差别是Value从80bytes定长变为80Bytes~1024Bytes之间的随机值，同时在程序正确性验证加入Crash-Recover的测试。

