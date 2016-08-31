#ifndef RSYNC_H
#define RSYNC_H

#include <iostream>
#include <unordered_map>
#include <string>
#include <functional>
#include <memory>
//文件数据块
struct DataBlock
{
    unsigned long long  offset;
    unsigned long long  adler32;
    std::string md5;
    DataBlock * next;
};

typedef std::shared_ptr<DataBlock> DataBlockPtr;

class Rsync
{
public:
    Rsync(std::string &filename);
private:
    void rollChecksum();
    //adler32和数据块的映射
    std::unordered_map<unsigned long long,DataBlockPtr> adler_blockMaps;
};

#endif // RSYNC_H
