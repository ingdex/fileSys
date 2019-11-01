//
// Created by ingdex on 19-2-25.
//

#include "filesys.h"
#include <iostream>
#include <string>
#include <fstream>
#include <cstring>
//#include <cstddef>

using namespace std;

FileSys::FileSys(string filename)
{
    volumeName = filename;
    fstream volume(filename);
    if (!volume.is_open())
    {
        cout << "can't open file volume!" << endl;
        exit(0);
    }
//    logined = 0;    //未登录
    init();
    volume.seekg(BLOCK_SIEZ);   //第一块为引导块，跳过
    volume.read((char *)&managementBlock, sizeof(ManagementBlock));
//    if (managementBlock.s_flag != S_FLAG)
//    {
//        //第一次挂在文件卷时初始化
////        logined = 1;
//        init();
////        logined = 0;
//        volume.seekg(BLOCK_SIEZ);
//        volume.read((char *)&managementBlock, sizeof(ManagementBlock));
//    }
//    volume.read((char *)&inodeBitMap, sizeof(InodeBitMap));
//    volume.seekg(BIT_MAP * BLOCK_SIEZ);
//    volume.read((char *)&bitmap, sizeof(BitMap));
    volume.close();
}

FileSys::~FileSys()
{

}

int FileSys::init()
{
//    if (!logined)
//    {
//        cout << "请先登录" << endl;
//        return -1;
//    }
    unsigned long long volumeSize;
    unsigned int s_flag;     //标识该文件卷是否是第一次挂载
    unsigned int s_isize;    //i结点区总块数
    unsigned int s_fsize;    //文件卷总块数
    unsigned int s_nfree;    //空闲块数
    unsigned int s_free[100];//空闲块块号数组
    unsigned int s_flock;    //空闲块锁
    unsigned int s_ninode;   //空闲i结点数
    unsigned int s_inode[15];//空闲i节点号数组
    unsigned int s_ilock;    //空闲i节点锁
    unsigned int s_fmod;     //修改标志
    unsigned int s_ronly;    //只读标志
    time_t s_time;           //最近修改时间
    fstream volume(volumeName);
    if (!volume.is_open())
    {
        cout << "open failed" << endl;
        exit(0);
    }
    char zero[BLOCK_SIEZ];
    memset(zero, 0, BLOCK_SIEZ);
    //初始化引导块
    volume.seekp(0, ios_base::beg);
    volume.write(zero, BLOCK_SIEZ);
    //初始化管理块
    //计算管理块数据
    //s_flag
    s_flag = S_FLAG;
    //s_isize;    //i结点区总块数
    volume.seekg(0, volume.end);
    volumeSize = volume.tellg();
    unsigned int blockNum = volumeSize/4096;  //文件卷块数
    unsigned int unitNum = volumeSize/(1024*64); //文件卷默认为固态硬盘，因为固态硬盘最小单元为64k，所以此公式可以计算出文件卷单元数
    /*
     * 固态硬盘最小单元为64k，系统中块大小为4k，古一个存储单元中有16块，拿出一块作为文件恢复时使用
     * 还剩15块，分配8个inode节点，根据ubuntu下sudo dumpe2fs -h /dev/hda | grep "Inode size"
     * 命令可知一个inode大小为256字节，因此每个固态盘存储单元需要256*8=2k字节大小的inode区，即每有两个
     * 固态盘最小单元就需要使用一个4k的块作为inode节点区。由此可计算inode区大小
     * */
    inodeBeg = 2;
    s_isize = unitNum*2;  //inode区块数
    //s_fsize;    //文件卷总块数
    s_fsize = unitNum*15;
    //成组链接法构建空闲块结构
    unsigned int blockLeft = s_fsize;
    unsigned int blockId = blockNum-1;
    //最后一块
    if (blockLeft >= 99)
    {
        s_nfree = 100;
        s_free[0] = 0;
        for (int i=99; i>0; i--)
        {
            if (!((blockId+1)%16)) //是存放恢复信息的块，每个64k大小的存储区域使用最后一位作为
            {
                blockId--;
            }
            s_free[i] = blockId--;
            blockLeft--;
        }
    }
    while (blockLeft > 0)
    {
        //将之前的空闲块信息写入新的空闲块
        if (!((blockId+1)%16)) //是存放恢复信息的块
        {
            blockId--;
        }
        volume.seekp(blockId*BLOCK_SIEZ, ios_base::beg);
        volume.write((char *)&s_nfree, sizeof(s_nfree));
        volume.write((char *)s_free, sizeof(unsigned int)*100);
        s_free[0] = blockId--;
        if (blockLeft >= 100)
        {
            s_nfree = 100;
            for (int i=99; i>0; i--)
            {
                if (!((blockId+1)%16)) //是存放恢复信息的块
                {
                    blockId--;
                }
                s_free[i] = blockId--;
            }
            blockLeft -= 100;
        }
        else
        {
           s_nfree = blockLeft;
           for (int i=blockLeft-1; i>0; i--)
           {
               if (!((blockId+1)%16)) //是存放恢复信息的块
               {
                   blockId--;
               }
               s_free[i] = blockId--;
           }
           blockLeft = 0;
        }
    }
    //s_flock;    //空闲块锁
    //s_ninode;   //空闲i结点数
    //s_inode[100];//空闲i节点号数组
    //成组链接法构建空闲inode结构
    unsigned int inodeLeft = s_isize-1;//0号inode节点作为根目录节点，默认已占用
    unsigned int inodeId = inodeLeft;
    //最后一组
    s_inode[0] = 0;
    if (inodeLeft >= 14)
    {
        s_ninode = 15;
        for (int i=14; i>0; i--)
        {
            s_inode[i] = inodeId--;
            inodeLeft--;
        }
    }
    while (inodeLeft > 0)
    {
        volume.seekp(BLOCK_SIEZ*2+inodeId*256, ios_base::beg);
        volume.write((char *)&s_ninode, sizeof(unsigned int));
        volume.write((char *)s_inode, sizeof(unsigned int)*15);
        s_inode[0] = inodeId--;
        if (inodeLeft >= 15)
        {
            s_ninode = 15;
            inodeLeft -= 15;
            for (int i=14; i>0; i--)
            {
                s_inode[i] = inodeId--;
            }
        }
        else
        {
            s_ninode = inodeLeft;
            for (int i=inodeLeft-1; i>0; i--)
            {
                s_inode[i] = inodeId--;
            }
            inodeLeft = 0;
        }
    }
    //s_ilock;    //空闲i节点锁
    //s_fmod;     //修改标志
    //s_ronly;    //只读标志
    //s_time;     //最近修改时间
    time(&s_time);
    managementBlock.s_flag = S_FLAG;
    managementBlock.s_isize = s_isize;
    managementBlock.s_fsize = s_fsize;
    managementBlock.s_nfree = s_nfree;
    for (int i=0; i<100; i++)
    {
        managementBlock.s_free[i] = s_free[i];
    }
    managementBlock.s_ninode = s_ninode;
    for (int i=0; i<15; i++)
    {
        managementBlock.s_inode[i] = s_inode[i];
    }
    managementBlock.s_time = s_time;
    volume.seekp(BLOCK_SIEZ, ios_base::beg);
    volume.write((char *)&managementBlock, sizeof(ManagementBlock));

    //创建根目录。0号inode节点为根目录目录文件
    Inode inode;
    memset((void *)&inode, 0, sizeof(Inode));
    inode.i_type = 0;
    inode.i_ilink = 1;
    inode.i_addr[0] = mallocBlock();
    inode.i_blksize = 4096;
    inode.i_size = 4096;
    inode.i_blocks = 1;
    inode.i_bytes = ENTRY_SIZE;
    //创建./目录
    DirEntry entry;
    entry.inode = 0;
    entry.file_type = 0;
    entry.name_len = 1;
    strcpy(entry.name, ".");
    volume.seekg(inode.i_addr[0]*BLOCK_SIEZ, ios_base::beg);
    volume.write((char *)&entry, sizeof(DirEntry));
    volume.seekg(inodeBeg*BLOCK_SIEZ, ios_base::beg);
    volume.write((char *)&inode, sizeof(Inode));
    updateManegementBlock();
    volume.close();

    return 0;
}

unsigned int FileSys::mallocBlock()
{
    managementBlock.s_nfree--;
    fstream volume(volumeName);
    if (!volume.is_open())
    {
        cout << "open failed" << endl;
        exit(0);
    }
    if (managementBlock.s_nfree == 0)
    {
        unsigned int freeBlk = managementBlock.s_free[0];
        unsigned int s_nfree, s_free[100];
        if (freeBlk == 0)
        {
            //当前没有空闲块
            return -1;
        }
        //将s_free[0]指向的空闲块读入管理块

        volume.seekg(freeBlk*BLOCK_SIEZ, ios_base::beg);
        volume.read((char *)&s_nfree, sizeof(unsigned int));
        volume.read((char *)s_free, sizeof(unsigned int)*100);
        managementBlock.s_nfree = s_nfree;
        for (int i=0; i<100; i++) {
            managementBlock.s_free[i] = s_free[i];
        }
        char zero[BLOCK_SIEZ];
        memset((void *)zero, 0, BLOCK_SIEZ);
//        DirEntry entrys[8];
//        getEntryFromBlk(entrys, freeBlk);
        volume.seekp(freeBlk*BLOCK_SIEZ, ios_base::beg);
        volume.write(zero, BLOCK_SIEZ);
//        getEntryFromBlk(entrys, freeBlk);
        return freeBlk;
    }
    else {
        unsigned int freeBlk = managementBlock.s_free[managementBlock.s_nfree];
        volume.seekp(freeBlk*BLOCK_SIEZ, ios_base::beg);
        char zero[BLOCK_SIEZ];
        memset((void *)zero, 0, BLOCK_SIEZ);
        volume.write(zero, BLOCK_SIEZ);
        return managementBlock.s_free[managementBlock.s_nfree];
    }
}

int FileSys::createFile(string filename, char type, int uid, int parentInodeNum)
{
//    if (!logined)
//    {
//        cout << "请先登录" << endl;
//        return -1;
//    }
    fstream volume(volumeName);
    if (!volume.is_open())
    {
        cout << "open failed" << endl;
        exit(0);
    }
    //查找文件是否已存在
    DirEntry entry;
    unsigned int addr = 0, entryCount = 0;
    Inode parentInode = getInode(parentInodeNum);
    //entryCount = getDataBlocksCount(parentInode.i_blocks);
    //直接索引
    for (int i=0; i<10; i++)
    {
        if ((addr = parentInode.i_addr[i]) == 0)
        {   //文件不存在，进入下一步操作
            break;
        }
        DirEntry entrys[8];
        getEntryFromBlk(entrys, addr);
        if (findFileInEntryBlk(entrys, filename))
            return -1;
    }
    //一级间接索引
    if (parentInode.i_addr[10] != 0) {
        unsigned int addr1[1024];
        //一次读出所有一级间接索引
        volume.seekg(parentInode.i_addr[10]*BLOCK_SIEZ, ios_base::beg);
        volume.read((char *)addr1, sizeof(unsigned int)*1024);
        for (int i=0; i<1024; i++) {
            addr = addr1[i];
            if (addr == 0)
                break;
            DirEntry entrys[8];
            getEntryFromBlk(entrys, addr);
            if (findFileInEntryBlk(entrys, filename))
                return -1;
        }
    }

    //二级间接索引
    if (parentInode.i_addr[11] != 0) {
        unsigned int addr1[1024];
        //一次读出所有二级索引
        volume.seekg(parentInode.i_addr[10]*BLOCK_SIEZ, ios_base::beg);
        volume.read((char *)addr1, sizeof(unsigned int)*1024);
        for (int i=0; i<1024; i++) {
            addr = addr1[i];
            if (addr == 0)
                break;
            //读取一级索引
            volume.seekg(addr * BLOCK_SIEZ);
            unsigned int addr2[1024];
            volume.read((char *)addr2, sizeof(unsigned int)*1024);
            for (int j=0; j<1024; j++) {
                addr = addr2[j];
                if (addr == 0) {
                    break;
                }
                DirEntry entrys[8];
                getEntryFromBlk(entrys, addr);
                if (findFileInEntryBlk(entrys, filename))
                    return -1;
            }

        }
    }
    //三级间接索引
    if (parentInode.i_addr[12] != 0) {
        unsigned int addr1[1024];
        //一次读出所有三级索引
        volume.seekg(parentInode.i_addr[10]*BLOCK_SIEZ, ios_base::beg);
        volume.read((char *)addr1, sizeof(unsigned int)*1024);
        for (int i=0; i<1024; i++) {
            addr = addr1[i];
            if (addr == 0)
                break;
            //读取二级索引
            volume.seekg(addr * BLOCK_SIEZ);
            unsigned int addr2[1024];
            volume.read((char *)addr2, sizeof(unsigned int)*1024);
            for (int j=0; j<1024; j++) {
                addr = addr2[j];
                if (addr == 0) {
                    break;
                }
                //读取一级索引
                unsigned int addr3[1024];
                volume.read((char *)addr3, sizeof(unsigned int)*1024);
                volume.seekg(addr * BLOCK_SIEZ);
                for(int k=0; k<1024; k++) {
                    addr = addr3[k];
                    if (addr == 0)
                        break;
                    DirEntry entrys[8];
                    getEntryFromBlk(entrys, addr);
                    if (findFileInEntryBlk(entrys, filename))
                        return -1;
                }
            }

        }
    }

    //增加inode
    int inodeNum = mallocInode();
    if (inodeNum <= 0)
    {
        cout << "i结点不足" << endl;
        return -1;
    }
    Inode inode;
    inode.i_type = type;
//    inode.i_uid = uid;
    inode.i_ilink = 1;
    inode.i_addr[0] = mallocBlock();
    if (inode.i_addr[0] < 0)
    {
        freeInode(inodeNum);
        cout << "空闲块数不足" << endl;
        return -1;
    }
    for (int i=1; i<13; i++)
    {
        inode.i_addr[i] = 0;
    }
    inode.i_blksize = 4096;
    inode.i_size = 4096;
    inode.i_blocks = 1;
    inode.i_bytes = 0;
    volume.seekp(inodeBeg*BLOCK_SIEZ + inodeNum*INODE_SIZE, ios_base::beg);
    volume.write((char *)&inode, sizeof(Inode));

    //初始化新文件i_addr[0]
    if (type == DIR)
    {   //如果为目录文件，写入./和../目录项
        inode.i_bytes = ENTRY_SIZE*2;
        memset((void *)&entry, 0, sizeof(DirEntry));
        entry.inode = inodeNum;
        entry.name_len = 1;
        strcpy(entry.name, ".");
        volume.seekp(inode.i_addr[0] * BLOCK_SIEZ, ios_base::beg);
        volume.write((char *)&entry, sizeof(DirEntry));
        entry.inode = parentInodeNum;
        entry.name_len = 2;
        strcpy(entry.name, "..");
        volume.seekp(inode.i_addr[0] * BLOCK_SIEZ + ENTRY_SIZE, ios_base::beg);
        volume.write((char *)&entry, sizeof(DirEntry));
        //debug
        DirEntry entrys[8];
        getEntryFromBlk(entrys, inode.i_addr[0]);
//        volume.seekg(inode.i_addr[0]*BLOCK_SIEZ, ios_base::beg);
//        volume.read((char *)entrys, BLOCK_SIEZ);
        entrys[0].inode = 1;
    }
    //如果非目录文件，分配一块空闲块即可，初始文件大小为0，无需处理
    //在父目录下建立目录项
    Inode inodeParent = getInode(parentInodeNum);
    //构建目录项
    entry.inode = inodeNum;
    strcpy(entry.name, filename.c_str());
    entry.name_len = filename.length();
    //将目录项写入父目录块，考虑需要增加间接索引情况
    //找到目录项要写入的块
    //直接索引
    for (int i=0; i<10; i++)
    {
        addr = inodeParent.i_addr[i];
        if (addr == 0)
        {   //需要增加块
            addr = mallocBlock();
            if (addr <= 0)
            {
                cout << "块数目不足" << endl;
                return -1;
            }
            inodeParent.i_addr[i] = addr;
            inodeParent.i_size += ENTRY_SIZE;
            inodeParent.i_blocks++;
            inodeParent.i_bytes = ENTRY_SIZE;
            writeInode(inodeParent, parentInodeNum);
            //写入目录项
            volume.seekp(addr*BLOCK_SIEZ, ios_base::beg);
            volume.write((char *)&entry, sizeof(DirEntry));
            //debug
            volume.seekg(addr*BLOCK_SIEZ, ios_base::beg);
            volume.read((char *)&entry, sizeof(DirEntry));
            return inodeNum;
        }
        else {
            //读取一块
            DirEntry entrys[8];
            getEntryFromBlk(entrys, addr);
//            volume.seekg(addr*BLOCK_SIEZ, ios_base::beg);
//            volume.read((char *)entrys, BLOCK_SIEZ);
            for (int i=0; i<8; i++) {
                DirEntry tmp = entrys[i];
                if (tmp.inode==0 && tmp.name_len==0)
                {
                    inodeParent.i_size += ENTRY_SIZE;
                    inodeParent.i_bytes += ENTRY_SIZE;
                    writeInode(inodeParent, parentInodeNum);
//                    entrys[i] = entry;
                    volume.seekp(addr*BLOCK_SIEZ+i*ENTRY_SIZE, ios_base::beg);
                    volume.write((char *)&entry, BLOCK_SIEZ);
                    return inodeNum;
                }
            }
        }
    }
    //一级间接索引
    addr = inodeParent.i_addr[10];
    unsigned int addr2[1014];
    //读入一级间接索引
    volume.seekg(addr*BLOCK_SIEZ, ios_base::beg);
    volume.read((char *)addr2, sizeof(unsigned int)*1024);
    for(int i=0; i<1024; i++) {
        unsigned int addr1[1024];
        addr = addr2[i];
        if (addr == 0) {
            addr = mallocBlock();
            if (addr <= 0) {
                cout << "块数目不足" << endl;
                return -1;
            }
            addr2[i] = addr;
            addr = mallocBlock();
            if (addr <= 0) {
                cout << "块数目不足" << endl;
                return -1;
            }
            //写入目录项
            volume.seekp(addr*BLOCK_SIEZ, ios_base::beg);
            volume.write((char *)&entry, sizeof(DirEntry));
            addr = inodeParent.i_addr[10];
            //写addr2[i]
            volume.seekg(addr*BLOCK_SIEZ+i* sizeof(unsigned int), ios_base::beg);
            volume.write((char *)&addr2[i], sizeof(unsigned int));
            //写父目录inode
            inodeParent.i_size += ENTRY_SIZE;
            inodeParent.i_blocks++;
            inodeParent.i_bytes += ENTRY_SIZE;
            writeInode(inodeParent, parentInodeNum);
            return inodeNum;
        }
        //不是最后一块有数据的直接索引
        else if (i<99 && addr2[i+1]!=0) {
            continue;
        }
        else {
            //读取直接索引
            volume.seekg(addr2[i]*BLOCK_SIEZ, ios_base::beg);
            volume.read((char *)&addr1, sizeof(unsigned int)*1024);
            for (int j=0; j<1024; j++) {
                addr = addr1[j];
                if (addr == 0) {
                    addr = mallocBlock();
                    if (addr <= 0) {
                        cout << "块数目不足" << endl;
                        return -1;
                    }
                    addr1[j] = addr;
                    volume.seekp(addr*BLOCK_SIEZ, ios_base::beg);
                    volume.write((char *)&entry, sizeof(DirEntry));
                    inodeParent.i_size += BLOCK_SIEZ;
                    inodeParent.i_blocks++;
                    inodeParent.i_bytes = ENTRY_SIZE;
                    writeInode(inodeParent, parentInodeNum);
                    return inodeNum;
                }
                else if (j<1023 && addr1[j+1]!=0){
                    continue;
                }
                else {
                    //读取一块
                    DirEntry entrys[8];
                    getEntryFromBlk(entrys, addr);
//                    volume.seekg(addr*BLOCK_SIEZ, ios_base::beg);
//                    volume.read((char *)entrys, BLOCK_SIEZ);
                    for (int i=0; i<8; i++) {
                        DirEntry tmp = entrys[i];
                        if (tmp.inode == 0)
                        {
                            inodeParent.i_size += ENTRY_SIZE;
                            inodeParent.i_bytes += ENTRY_SIZE;
                            writeInode(inodeParent, parentInodeNum);
                            entrys[i] = entry;
                            volume.seekp(addr*BLOCK_SIEZ+i*sizeof(DirEntry), ios_base::beg);
                            volume.write((char *)entrys, BLOCK_SIEZ);
                            return inodeNum;
                        }
                    }
                }
            }
        }
    }
    //二级间接索引
    addr = inodeParent.i_addr[12];
    unsigned int addr3[1014];
    //读入二级间接索引
    volume.seekg(addr*BLOCK_SIEZ, ios_base::beg);
    volume.read((char *)addr2, sizeof(unsigned int)*1024);
    for(int i=0; i<1024; i++) {
        unsigned int addr1[1024];
        addr = addr2[i];
        if (addr == 0) {
            addr = mallocBlock();
            if (addr <= 0) {
                cout << "块数目不足" << endl;
                return -1;
            }
            addr2[i] = addr;
            addr = mallocBlock();
            if (addr <= 0) {
                cout << "块数目不足" << endl;
                return -1;
            }
            //写入目录项
            volume.seekp(addr*BLOCK_SIEZ, ios_base::beg);
            volume.write((char *)&entry, sizeof(DirEntry));
            addr = inodeParent.i_addr[10];
            //写addr2[i]
            volume.seekg(addr*BLOCK_SIEZ+i* sizeof(unsigned int), ios_base::beg);
            volume.write((char *)&addr2[i], sizeof(unsigned int));
            //写父目录inode
            inodeParent.i_size += ENTRY_SIZE;
            inodeParent.i_blocks++;
            inodeParent.i_bytes += ENTRY_SIZE;
            writeInode(inodeParent, parentInodeNum);
            return inodeNum;
        }
            //不是最后一块有数据的直接索引
        else if (i<99 && addr2[i+1]!=0) {
            continue;
        }
        else {
            //读取一级索引
            volume.seekg(addr2[i]*BLOCK_SIEZ, ios_base::beg);
            volume.read((char *)&addr1, sizeof(unsigned int)*1024);
            for (int j=0; j<1024; j++) {
                addr = addr1[j];
                if (addr == 0) {
                    addr = mallocBlock();
                    if (addr <= 0) {
                        cout << "块数目不足" << endl;
                        return -1;
                    }
                    addr1[j] = addr;
                    volume.seekp(addr*BLOCK_SIEZ, ios_base::beg);
                    volume.write((char *)&entry, sizeof(DirEntry));
                    inodeParent.i_size += BLOCK_SIEZ;
                    inodeParent.i_blocks++;
                    inodeParent.i_bytes = ENTRY_SIZE;
                    writeInode(inodeParent, parentInodeNum);
                    return inodeNum;
                }
                else if (j<1023 && addr1[j+1]!=0){
                    continue;
                }
                else {
                    //读取一块
                    DirEntry entrys[8];
                    getEntryFromBlk(entrys, addr);
//                    volume.seekg(addr*BLOCK_SIEZ, ios_base::beg);
//                    volume.read((char *)entrys, BLOCK_SIEZ);
                    for (int i=0; i<8; i++) {
                        DirEntry tmp = entrys[i];
                        if (tmp.inode == 0)
                        {
                            inodeParent.i_size += ENTRY_SIZE;
                            inodeParent.i_bytes += ENTRY_SIZE;
                            writeInode(inodeParent, parentInodeNum);
                            entrys[i] = entry;
                            volume.seekp(addr*BLOCK_SIEZ+i*sizeof(DirEntry), ios_base::beg);
                            volume.write((char *)entrys, BLOCK_SIEZ);
                            return inodeNum;
                        }
                    }
                }
            }
        }
    }

    //debug
    volume.seekg(addr*BLOCK_SIEZ, ios_base::beg);
    volume.read((char *)&entryCount, sizeof(int));

    volume.seekp(addr*BLOCK_SIEZ + sizeof(int) + (entryCount-1)*sizeof(DirEntry), ios_base::beg);
    volume.write((char *)&entry, sizeof(DirEntry));

    //debug
    volume.seekg(addr*BLOCK_SIEZ + sizeof(int) + (entryCount-1)*sizeof(DirEntry), ios_base::beg);
    volume.read((char *)&entry, sizeof(DirEntry));

    volume.close();

    return inodeNum;
}

void FileSys::getEntryFromBlk(DirEntry entry[8], unsigned int addr) {
    fstream volume(volumeName);
    if (!volume.is_open())
    {
        cout << "open failed" << endl;
        exit(0);
    }
    for (int i=0; i<8; i++)
    {
        DirEntry tmp;
        volume.seekg(addr*BLOCK_SIEZ+i*ENTRY_SIZE, ios_base::beg);
        volume.read((char *)&tmp, sizeof(DirEntry));
        entry[i] = tmp;
    }
    volume.close();
    return;
}

bool FileSys::findFileInEntryBlk(DirEntry entrys[8], string filename)
{
    DirEntry entry;
    for (int j=0; j<8; j++)
    {
        entry = entrys[j];
        if (entry.inode==0 && entry.name_len==0) {
            break;
        }
        if (strcmp(entry.name, filename.c_str()) == 0)
        {   //已有文件名为filename的文件
            Inode inode = getInode(entry.inode);
            if (inode.i_type == REG_FILE)
            {
                cout << "已经存在普通文件" << filename << endl;
                return true;
            }
            else if (inode.i_type == DIR)
            {
                cout << "已经存在目录" << filename << endl;
                return true;
            }
        }
    }
    return false;
}

//
//int FileSys::deleteFile(int inodeNum) {}
//
//int FileSys::openFile(Inode inode) {}
//
//int FileSys::closeFile(Inode inode) {}
//
//int FileSys::readFile() {}
//
int FileSys::displayFile(int inodeNum)
{
    fstream volume(volumeName);
    if (!volume.is_open())
    {
        cout << "open failed" << endl;
        exit(0);
    }
    Inode inode = getInode(inodeNum);
    int i = 0, j= 0, addr = 0;
    int entryCount = 0; //目录项数目
    int charCount = 0;  //字符数目
    DirEntry entry[8];
    string str; //文件内容
    char content[BLOCK_SIEZ];
    memset(content, 0, BLOCK_SIEZ);
    if (inode.i_type == DIR)  //是目录文件
    {
        for (i=0; i<10; i++)
        {
            if ((addr = inode.i_addr[i]) == 0)
            {
                break;
            }
            getEntryFromBlk(entry, addr);
            for (int j=0; j<8; j++)
            {
                DirEntry tmp;
                tmp = entry[j];
                if (tmp.inode==0 && tmp.name_len==0)
                    break;
                cout << tmp.name << endl;
            }
        }
        //一级索引
        //二级索引
        //三级索引
    }
    else    //是普通文件
    {

//        for (i=0; i<10; i++)
//        {
//            if ((addr = inode.i_addr[i]) == 0)
//            {
//                break;
//            }
//            volume.seekg(addr*BLOCK_SIEZ, ios_base::beg);
//            volume.read((char *)&charCount, sizeof(int));
//            volume.read(content, charCount);
//            content[charCount] = '\0';
//            cout << content;
//            if (charCount < MAX_CHAR_COUNT)
//            {
//                break;
//            }
//
//            //未考虑普通文件存在间接索引情况
//
//        }
    }
    volume.close();

    return 0;
}

unsigned int FileSys::mallocInode()
{
    managementBlock.s_ninode--;
    if (managementBlock.s_ninode == 0)
    {
        if (managementBlock.s_inode[0] == 0)
        {
            return 0;
        }
        unsigned int addr = managementBlock.s_inode[0];
        fstream volume(volumeName);
        volume.seekg(inodeBeg*BLOCK_SIEZ+addr*INODE_SIZE, ios_base::beg);
        volume.read((char *)&managementBlock.s_ninode, sizeof(unsigned int));
        volume.read((char *)managementBlock.s_inode, sizeof(unsigned int)*15);
        return addr;
    }
    return managementBlock.s_inode[managementBlock.s_ninode];
}

int FileSys::cdDir(string dirName, unsigned int &curdir)
{
    fstream volume(volumeName);
    if (!volume.is_open())
    {
        cout << "open failed" << endl;
        exit(0);
    }
    unsigned int re = findFile(curdir, dirName);
    if (re != -1) curdir = re;
    return re;
//    Inode inodeParent = getInode(curdir);
//    //直接索引
//    for (int i=0; i<10; i++)
//    {
//        unsigned int addr = inodeParent.i_addr[i];
//        if (addr == 0)
//        {
//            cout << "file not exist" << endl;
//            return -1;
//        }
//        unsigned int tmp = findEntryBy0Index(addr, dirName);
//        if (tmp>=0 && tmp<=0xfffffffe)
//        {
//            Inode dir = getInode(tmp);
//            if (dir.i_type != DIR)
//            {
//                cout << dirName << "不是目录文件" << endl;
//                return -1;
//            }
//            curdir = tmp;
//            return 0;
//        }
//    }
//    //一级索引
//    unsigned int addr = inodeParent.i_addr[10];
//    if (addr == 0)
//    {
//        cout << "file not exist" << endl;
//        return -1;
//    }
//    unsigned int addr1[1014];
//    //读入一级索引
//    volume.seekg(addr*BLOCK_SIEZ, ios_base::beg);
//    volume.read((char *)addr1, sizeof(unsigned int)*1024);
//    for(int i=0; i<1024; i++) {
//        addr = addr1[i];
//        if (addr == 0) {
//            cout << "file not exist" << endl;
//            return -1;
//        }
//        unsigned int tmp = findEntryBy0Index(addr, dirName);
//        if (tmp>=0 && tmp<=0xfffffffe)
//        {
//            Inode dir = getInode(tmp);
//            if (dir.i_type != DIR)
//            {
//                cout << dirName << "不是目录文件" << endl;
//                return -1;
//            }
//            curdir = tmp;
//            return 0;
//        }
//    }
//    //二级索引
//    addr = inodeParent.i_addr[11];
//    if (addr == 0)
//    {
//        cout << "file not exist" << endl;
//        return -1;
//    }
//    unsigned int addr2[1014];
//    //读入二级间接索引
//    volume.seekg(addr*BLOCK_SIEZ, ios_base::beg);
//    volume.read((char *)addr2, sizeof(unsigned int)*1024);
//    for(int i=0; i<1024; i++) {
//        addr = addr2[i];
//        if (addr == 0) {
//            cout << "file not exist" << endl;
//            return -1;
//        }
//        //读取一级索引
//        volume.seekg(addr*BLOCK_SIEZ, ios_base::beg);
//        volume.read((char *)addr1, sizeof(unsigned int)*1024);
//        for(int j=0; j<1024; j++)
//        {
//            addr = addr1[j];
//            if (addr == 0)
//            {
//                cout << "file not exist" << endl;
//                return -1;
//            }
//            unsigned int tmp = findEntryBy0Index(addr, dirName);
//            if (tmp>=0 && tmp<=0xfffffffe)
//            {
//                Inode dir = getInode(tmp);
//                if (dir.i_type != DIR)
//                {
//                    cout << dirName << "不是目录文件" << endl;
//                    return -1;
//                }
//                curdir = tmp;
//                return 0;
//            }
//        }
//    }
//    //三级索引
//    addr = inodeParent.i_addr[12];
//    if (addr == 0)
//    {
//        cout << "file not exist" << endl;
//        return -1;
//    }
//    unsigned int addr3[1024];
//    //读入三级间接索引
//    volume.seekg(addr*BLOCK_SIEZ, ios_base::beg);
//    volume.read((char *)addr3, sizeof(unsigned int)*1024);
//    for(int i=0; i<1024; i++) {
//        addr = addr3[i];
//        if (addr == 0) {
//            cout << "file not exist" << endl;
//            return -1;
//        }
//        //读取二级索引
//        volume.seekg(addr*BLOCK_SIEZ, ios_base::beg);
//        volume.read((char *)addr2, sizeof(unsigned int)*1024);
//        for(int j=0; j<1024; j++)
//        {
//            addr = addr2[j];
//            if (addr == 0)
//            {
//                cout << "file not exist" << endl;
//                return -1;
//            }
//            //读取一级索引
//            volume.seekg(addr*BLOCK_SIEZ, ios_base::beg);
//            volume.read((char *)addr1, sizeof(unsigned int)*1024);
//            for (int k=0; k<1024; k++)
//            {
//                addr = addr1[j];
//                if (addr == 0)
//                {
//                    cout << "file not exist" << endl;
//                    return -1;
//                }
//                unsigned int tmp = findEntryBy0Index(addr, dirName);
//                if (tmp>=0 && tmp<=0xfffffffe)
//                {
//                    Inode dir = getInode(tmp);
//                    if (dir.i_type != DIR)
//                    {
//                        cout << dirName << "不是目录文件" << endl;
//                        return -1;
//                    }
//                    curdir = tmp;
//                    return 0;
//                }
//            }
//        }
//    }
//    cout << "file not exist" << endl;
//    return -1;
}

Inode FileSys::getInode(int inodeNum)
{
    fstream volume(volumeName);
    Inode inode;
    if (!volume.is_open())
    {
        cout << "open failed" << endl;
        exit(0);
    }
    volume.seekg(inodeBeg*BLOCK_SIEZ + inodeNum*INODE_SIZE, ios_base::beg);
    volume.read((char *)&inode, sizeof(Inode));
    volume.close();
    return inode;
}

int FileSys::writeInode(Inode inode, int inodeNum)
{
    fstream volume(volumeName);
    volume.seekp(inodeBeg*BLOCK_SIEZ + inodeNum*INODE_SIZE);
    volume.write((char *)&inode, sizeof(Inode));
    volume.close();
    return 0;
}

int FileSys::updateManegementBlock()
{
    fstream volume(volumeName);
    if (!(volume.is_open()))
    {
        cout << "文件卷打开失败" << endl;
        exit(-1);
    }
    //更新管理块
    volume.seekp(BLOCK_SIEZ, ios_base::beg);
    volume.write((char *)&managementBlock, sizeof(ManagementBlock));
    return 0;
}

int FileSys::writeFile(unsigned int parentInodeNum, string filename)
{
    fstream volume(volumeName);
    if (!volume.is_open())
    {
        cout << "failed to open file" << endl;
        return -1;
    }
    unsigned int inodeNum = findFile(parentInodeNum, filename);
    if (inodeNum == -1)
    {
        inodeNum = createFile(filename, REG_FILE, 0, parentInodeNum);
    }
    if (inodeNum == -1)
        return -1;
    Inode inode = getInode(inodeNum);
    if (inode.i_type != REG_FILE)
    {
        cout << filename << " 不是文本文件，不可写" << endl;
        return -1;
    }
    //写文本文件，以更新的方式
    //inode块数据清除
    inode.i_size = 0;
    inode.i_blocks = 0;
    inode.i_bytes = 0;
    FileHandle handle = {inode, inodeNum, 0, 0, 0, 0, 0};
    char buf[BLOCK_SIEZ];
    int charCount = 0;
    char c;
    int addrNum = 0;
    unsigned int addr = inode.i_addr[addrNum];   //要写入的文件的第一个存储块
    getchar();  //去掉命令中输入的换行符
    while ((c=getc(stdin)) != EOF)
    {
        //获取输入
        buf[charCount] = c;
        charCount++;
        //写入
        if (charCount == BLOCK_SIEZ)    //输入的字符数达到一个空闲块可存储的最多字符数
        {
            writeBlk(handle, buf, BLOCK_SIEZ);
        }
    }
    if (charCount == 0)
    {
        return 0;
    }
    writeBlk(handle, buf, charCount);
    //释放未用的存储块

    return 0;
}

//以块为单位写普通文件，除了最后一块之外默认其余块都是一整块
int FileSys::writeBlk(FileHandle &handle, char *buf, unsigned int size)
{
    unsigned int addr;
    fstream volume(volumeName);
    if (!volume.is_open())
    {
        cout << "failed to open file" << endl;
        return -1;
    }
    if (handle.index == 13)
    {
        cout << "文件已满" << endl;
        return -1;
    }
    //写入直接索引块
    if (handle.index < 10)
    {
        addr = handle.inode.i_addr[handle.index];
        if(addr == 0)
        {
            addr = mallocBlock();
            if (addr == -1)
            {   //空闲块分配失败
                cout << "文件卷已满" << endl;
                return -1;
            }
            else
            {
                handle.inode.i_addr[handle.index] = addr;
            }
        }
        handle.inode.i_size += size;
        handle.inode.i_blocks++;
        handle.inode.i_bytes = size;
        handle.pos = 0;
        handle.index++;
        volume.seekp(addr * BLOCK_SIEZ);
        volume.write(buf, size);
    }
    else if (handle.index == 10)//一级索引
    {
        unsigned int index1 = handle.inode.i_addr[10];
        if(index1 == 0)
        {
            index1 = mallocBlock();
            if (index1 == -1)
            {   //空闲块分配失败
                cout << "文件卷已满" << endl;
                return -1;
            }
            else
            {
                handle.inode.i_addr[handle.index] = index1;
            }
        }
        //读入直接索引
        unsigned int addr0[1024];
        volume.seekg(index1*BLOCK_SIEZ, ios_base::beg);
        volume.read((char *)addr0, BLOCK_SIEZ);
        unsigned int index0;
        index0 = addr0[handle.index0pos];
        if(index0 == 0)
        {
            index0 = mallocBlock();
            if (index0 == -1)
            {   //空闲块分配失败
                cout << "文件卷已满" << endl;
                return -1;
            }
            else
            {
                addr0[handle.index0pos] = index0;
                volume.seekp(handle.inode.i_addr[10]*BLOCK_SIEZ+handle.index0pos*4, ios_base::beg);
                volume.write((char*)&addr0[handle.index0pos], 4);
            }
        }
        volume.seekp(index0 * BLOCK_SIEZ);
        volume.write(buf, size);
        handle.inode.i_size += size;
        handle.inode.i_blocks++;
        handle.inode.i_bytes = size;
        handle.pos = 0;
        handle.index0pos++;
        if (handle.index0pos == 1024)
        {
            handle.index++;
            handle.index0pos = 0;
            handle.index1pos = 0;
            handle.pos = 0;
        }
    }
    else if (handle.index == 11)//二级索引
    {
        unsigned int index2 = handle.inode.i_addr[11];
        if(index2 == 0)   //如果二级索引为空，则分配二级索引
        {
            index2 = mallocBlock();
            if (index2 == -1)
            {   //空闲块分配失败
                cout << "文件卷已满" << endl;
                return -1;
            }
            else
            {
                handle.inode.i_addr[11] = index2;
            }
        }
        //读取所有一级索引
        unsigned int addr1[1024];
        volume.seekg(index2*BLOCK_SIEZ, ios_base::beg);
        volume.read((char *)addr1, BLOCK_SIEZ);
        unsigned int index1 = addr1[handle.index1pos];
        if (index1 == 0)
        {
            index1 = mallocBlock();
            if (index1 == -1)
            {   //空闲块分配失败
                cout << "文件卷已满" << endl;
                return -1;
            }
            else
            {
                addr1[handle.index1pos] = index1;
                volume.seekp(index2*BLOCK_SIEZ+handle.index1pos*4, ios_base::beg);
                volume.write((char *)&index1, 4);
            }
        }
        //读取直接索引
        unsigned int addr0[1024];
        volume.seekg(index1*BLOCK_SIEZ, ios_base::beg);
        volume.read((char *)addr0, BLOCK_SIEZ);
        unsigned int index0 = addr0[handle.index0pos];
        if (index0 == 0)
        {
            index0 = mallocBlock();
            if (index0 == -1)
            {   //空闲块分配失败
                cout << "文件卷已满" << endl;
                return -1;
            }
            else
            {
                addr0[handle.index0pos] = index0;
                volume.seekp(index1*BLOCK_SIEZ+handle.index0pos*4, ios_base::beg);
                volume.write((char *)&index0, 4);
            }
        }
        //写入数据
        volume.seekp(index0*BLOCK_SIEZ, ios_base::beg);
        volume.write((char *)buf, size);
        handle.inode.i_size += size;
        handle.inode.i_blocks++;
        handle.inode.i_bytes = size;
        handle.pos = 0;
        handle.index0pos++;
        if (handle.index0pos == 1024)
        {
            handle.index++;
            handle.index0pos = 0;
            handle.index1pos++;
            handle.pos = 0;
        }
        if (handle.index1pos == 1024)
        {
            handle.index = 12;
            handle.index2pos = 0;
            handle.index1pos = 0;
            handle.index0pos = 0;
            handle.pos = 0;
        }
    }
    else if (handle.index == 12)//三级索引
    {
        unsigned int index3 = handle.inode.i_addr[12];
        if(index3 == 0)   //如果三级索引为空，则分配三级索引
        {
            index3 = mallocBlock();
            if (index3 == -1)
            {   //空闲块分配失败
                cout << "文件卷已满" << endl;
                return -1;
            }
            else
            {
                handle.inode.i_addr[12] = index3;
            }
        }
        //读取所有二级索引
        unsigned int addr2[1024];
        volume.seekg(index3*BLOCK_SIEZ, ios_base::beg);
        volume.read((char *)addr2, BLOCK_SIEZ);
        unsigned int index2 = addr2[handle.index2pos];
        if(index2 == 0)   //如果二级索引为空，则分配二级索引
        {
            index2 = mallocBlock();
            if (index2 == -1)
            {   //空闲块分配失败
                cout << "文件卷已满" << endl;
                return -1;
            }
            else
            {
                handle.inode.i_addr[11] = index2;
            }
        }
        //读取所有一级索引
        unsigned int addr1[1024];
        volume.seekg(index2*BLOCK_SIEZ, ios_base::beg);
        volume.read((char *)addr1, BLOCK_SIEZ);
        unsigned int index1 = addr1[handle.index1pos];
        if (index1 == 0)
        {
            index1 = mallocBlock();
            if (index1 == -1)
            {   //空闲块分配失败
                cout << "文件卷已满" << endl;
                return -1;
            }
            else
            {
                addr1[handle.index1pos] = index1;
                volume.seekp(index2*BLOCK_SIEZ+handle.index1pos*4, ios_base::beg);
                volume.write((char *)&index1, 4);
            }
        }
        //读取直接索引
        unsigned int addr0[1024];
        volume.seekg(index1*BLOCK_SIEZ, ios_base::beg);
        volume.read((char *)addr0, BLOCK_SIEZ);
        unsigned int index0 = addr0[handle.index0pos];
        if (index0 == 0)
        {
            index0 = mallocBlock();
            if (index0 == -1)
            {   //空闲块分配失败
                cout << "文件卷已满" << endl;
                return -1;
            }
            else
            {
                addr0[handle.index0pos] = index0;
                volume.seekp(index1*BLOCK_SIEZ+handle.index0pos*4, ios_base::beg);
                volume.write((char *)&index0, 4);
            }
        }
        //写入数据
        volume.seekp(index0*BLOCK_SIEZ, ios_base::beg);
        volume.write((char *)buf, size);
        handle.inode.i_size += size;
        handle.inode.i_blocks++;
        handle.inode.i_bytes = size;
        handle.pos = 0;
        handle.index0pos++;
        if (handle.index0pos == 1024)
        {
            handle.index++;
            handle.index0pos = 0;
            handle.index1pos++;
            handle.pos = 0;
        }
        if (handle.index1pos == 1024)
        {
            handle.index2pos++;
            handle.index1pos = 0;
            handle.index0pos = 0;
            handle.pos = 0;
        }
        if (handle.index2pos == 1024)
        {
            handle.index = 13;
        }
    }
    else
    {
        cout << "文件已满" << endl;
    }
    //更新句柄
    writeInode(handle.inode, handle.inodeNum);
    return 0;
}

int FileSys::readBlk(FileHandle &handle, char *buf)
{
//    unsigned int i_blocks = 0;  //当前已经读取的块数目
//    if (handle.index <= 9)
//    {
//        i_blocks = handle.index+1;
//    }
//    else if (handle.index == 10)
//    {
//        i_blocks = 10+handle.index0pos+1;
//    }
//    else if (handle.index == 11)
//    {
//        i_blocks = 10+1024+handle.index1pos*1024+handle.index0pos+1;
//    }
//    else if (handle.index == 12)
//    {
//        i_blocks = 10+1024+handle.index2pos*1024*1024+handle.index1pos*1024+handle.index0pos+1;
//    }
//    else if (handle.index == 13)
//    {
//        return 0;
//    }
//    else
//    {
//        cout << "bug" << endl;
//    }
    fstream volume(volumeName);
    if (!volume.is_open())
    {
        cout << "failed to open file" << endl;
        return -1;
    }
    if (handle.blocks > handle.inode.i_blocks)
    {
        return 0;
    }
    if (handle.blocks==handle.inode.i_blocks && handle.pos>handle.inode.i_bytes)
    {
        return 0;
    }
    if (handle.index < 10)
    {
        unsigned int index0 = handle.inode.i_addr[handle.index++];
        volume.seekg(index0*BLOCK_SIEZ, ios_base::beg);
        volume.read((char *)buf, BLOCK_SIEZ);
        if (handle.blocks == handle.inode.i_blocks)//读取的是文件的最后一块，特殊处理
        {
            if (handle.inode.i_bytes < BLOCK_SIEZ)
            {
                buf[handle.inode.i_bytes-1] = '\0';
                handle.blocks++;
                handle.pos = handle.inode.i_bytes;
                return handle.pos;
            }
            else
            {
                handle.blocks++;
            }
        }
    }
    else if (handle.index == 10)//一级索引
    {
        unsigned int index1 = handle.inode.i_addr[10];
        unsigned int addr0[1024];
        volume.seekg(index1*BLOCK_SIEZ, ios_base::beg);
        volume.read((char *)addr0, BLOCK_SIEZ);
        unsigned int index0;
        index0 = addr0[handle.index0pos];
        volume.seekg(index0 * BLOCK_SIEZ);
        volume.read((char *)buf, BLOCK_SIEZ);
        handle.blocks++;
        if (handle.blocks==handle.inode.i_blocks && handle.inode.i_bytes<BLOCK_SIEZ)
        {
            handle.pos = handle.inode.i_bytes;
            return 0;
        }
        handle.pos = 0;
        handle.index0pos++;
        if (handle.index0pos == 1024)
        {
            handle.index++;
            handle.index0pos = 0;
            handle.index1pos = 0;
            handle.pos = 0;
        }
    }
    else if (handle.index == 11)//二级索引
    {
        unsigned int index2 = handle.inode.i_addr[11];
        //读取一级索引
        unsigned int addr1[1024];
        volume.seekg(index2*BLOCK_SIEZ, ios_base::beg);
        volume.read((char *)addr1, BLOCK_SIEZ);
        unsigned int index1 = addr1[handle.index1pos];
        //读直接索引
        unsigned int addr0[1024];
        volume.seekg(index1*BLOCK_SIEZ, ios_base::beg);
        volume.read((char *)addr0, BLOCK_SIEZ);
        unsigned int index0;
        index0 = addr0[handle.index0pos];
        volume.seekg(index0 * BLOCK_SIEZ);
        volume.read((char *)buf, BLOCK_SIEZ);
        handle.blocks++;
        if (handle.blocks==handle.inode.i_blocks && handle.inode.i_bytes<BLOCK_SIEZ)
        {
            handle.pos = handle.inode.i_bytes;
            return 0;
        }
        handle.pos = 0;
        handle.index0pos++;
        if (handle.index0pos == 1024)
        {
            handle.index1pos++;
            handle.index0pos = 0;
            handle.pos = 0;
        }
        if (handle.index1pos == 1024)
        {
            handle.index1pos = 0;
            handle.index2pos = 0;
            handle.pos = 0;
            handle.index++;
        }
    }
    else if (handle.index == 12)//三级索引
    {
        unsigned int index3 = handle.inode.i_addr[12];
        //读取二级索引
        unsigned int addr2[1024];
        volume.seekg(index3*BLOCK_SIEZ, ios_base::beg);
        volume.read((char *)addr2, BLOCK_SIEZ);
        unsigned int index2 = addr2[handle.index2pos];
        //读取一级索引
        unsigned int addr1[1024];
        volume.seekg(index2*BLOCK_SIEZ, ios_base::beg);
        volume.read((char *)addr1, BLOCK_SIEZ);
        unsigned int index1 = addr1[handle.index1pos];
        //读直接索引
        unsigned int addr0[1024];
        volume.seekg(index1*BLOCK_SIEZ, ios_base::beg);
        volume.read((char *)addr0, BLOCK_SIEZ);
        unsigned int index0;
        index0 = addr0[handle.index0pos];
        volume.seekg(index0 * BLOCK_SIEZ);
        volume.read((char *)buf, BLOCK_SIEZ);
        handle.blocks++;
        if (handle.blocks==handle.inode.i_blocks && handle.inode.i_bytes<BLOCK_SIEZ)
        {
            handle.pos = handle.inode.i_bytes;
            return 0;
        }
        handle.pos = 0;
        handle.index0pos++;
        if (handle.index0pos == 1024)
        {
            handle.index1pos++;
            handle.index0pos = 0;
            handle.pos = 0;
        }
        if (handle.index1pos == 1024)
        {
            handle.index1pos = 0;
            handle.index2pos++;
            handle.pos = 0;
        }
        if (handle.index2pos == 1024)
        {
            handle.index++;
        }
    }
    else
    {
        cout << "文件已满" << endl;
    }
    //更新句柄
    writeInode(handle.inode, handle.inodeNum);
    return 0;
}

int FileSys::catFile(unsigned int parentInodeNum, string filename)
{
    fstream volume(volumeName);
    if (!volume.is_open())
    {
        cout << "failed to open file" << endl;
        return -1;
    }
    unsigned int inodeNum = findFile(parentInodeNum, filename);
    Inode inode = getInode(inodeNum);
    if (inode.i_type != REG_FILE)
    {
        cout << "文件格式错误" << endl;
        return -1;
    }
    FileHandle handle = {inode, inodeNum, 0, 0, 0, 0, 0, 0};
    unsigned int charCount = 0;
    char buf[BLOCK_SIEZ+1];
    buf[BLOCK_SIEZ] = '\0';
    while ((charCount = readBlk(handle, buf)) > 0)
    {
        cout << buf;
    }
    return 0;
}

unsigned int FileSys::findFile(unsigned int parentInodeNum, string filename)
{
    fstream volume(volumeName);
    if (!volume.is_open())
    {
        cout << "open failed" << endl;
        exit(0);
    }
    Inode inodeParent = getInode(parentInodeNum);
    //直接索引
    for (int i=0; i<10; i++)
    {
        unsigned int addr = inodeParent.i_addr[i];
        if (addr == 0)
        {
            cout << "file not exist" << endl;
            return -1;
        }
        unsigned int tmp = findEntryBy0Index(addr, filename);
        if (tmp>=0 && tmp<=0xfffffffe)
        {
            return tmp;
        }
    }
    //一级索引
    unsigned int addr = inodeParent.i_addr[10];
    if (addr == 0)
    {
        cout << "file not exist" << endl;
        return -1;
    }
    unsigned int addr1[1014];
    //读入一级索引
    volume.seekg(addr*BLOCK_SIEZ, ios_base::beg);
    volume.read((char *)addr1, sizeof(unsigned int)*1024);
    for(int i=0; i<1024; i++) {
        addr = addr1[i];
        if (addr == 0) {
            cout << "file not exist" << endl;
            return -1;
        }
        unsigned int tmp = findEntryBy0Index(addr, filename);
        if (tmp>=0 && tmp<=0xfffffffe)
        {
            return tmp;
        }
    }
    //二级索引
    addr = inodeParent.i_addr[11];
    if (addr == 0)
    {
        cout << "file not exist" << endl;
        return -1;
    }
    unsigned int addr2[1014];
    //读入二级间接索引
    volume.seekg(addr*BLOCK_SIEZ, ios_base::beg);
    volume.read((char *)addr2, sizeof(unsigned int)*1024);
    for(int i=0; i<1024; i++) {
        addr = addr2[i];
        if (addr == 0) {
            cout << "file not exist" << endl;
            return -1;
        }
        //读取一级索引
        volume.seekg(addr*BLOCK_SIEZ, ios_base::beg);
        volume.read((char *)addr1, sizeof(unsigned int)*1024);
        for(int j=0; j<1024; j++)
        {
            addr = addr1[j];
            if (addr == 0)
            {
                cout << "file not exist" << endl;
                return -1;
            }
            unsigned int tmp = findEntryBy0Index(addr, filename);
            if (tmp>=0 && tmp<=0xfffffffe)
            {
                return tmp;
            }
        }
    }
    //三级索引
    addr = inodeParent.i_addr[12];
    if (addr == 0)
    {
        cout << "file not exist" << endl;
        return -1;
    }
    unsigned int addr3[1024];
    //读入三级间接索引
    volume.seekg(addr*BLOCK_SIEZ, ios_base::beg);
    volume.read((char *)addr3, sizeof(unsigned int)*1024);
    for(int i=0; i<1024; i++) {
        addr = addr3[i];
        if (addr == 0) {
            cout << "file not exist" << endl;
            return -1;
        }
        //读取二级索引
        volume.seekg(addr*BLOCK_SIEZ, ios_base::beg);
        volume.read((char *)addr2, sizeof(unsigned int)*1024);
        for(int j=0; j<1024; j++)
        {
            addr = addr2[j];
            if (addr == 0)
            {
                cout << "file not exist" << endl;
                return -1;
            }
            //读取一级索引
            volume.seekg(addr*BLOCK_SIEZ, ios_base::beg);
            volume.read((char *)addr1, sizeof(unsigned int)*1024);
            for (int k=0; k<1024; k++)
            {
                addr = addr1[j];
                if (addr == 0)
                {
                    cout << "file not exist" << endl;
                    return -1;
                }
                unsigned int tmp = findEntryBy0Index(addr, filename);
                if (tmp>=0 && tmp<=0xfffffffe)
                {
                    return tmp;
                }
            }
        }
    }
    cout << "file not exist" << endl;
    return -1;
}

unsigned int FileSys::findEntryBy0Index(unsigned int addr, string dirName)
{
    if (addr == 0)
        return -1;
    fstream volume(volumeName);
    if (!volume.is_open())
    {
        cout << "open failed" << endl;
        exit(0);
    }
    DirEntry entry[8];
    getEntryFromBlk(entry, addr);
    for (int j=0; j<8; j++)
    {
        DirEntry tmp = entry[j];
        if (tmp.inode == 0 && tmp.name_len==0)
        {
            return -1;
        }
        if (strcmp(tmp.name, dirName.c_str()) == 0)
        {
            return tmp.inode;
        }
    }
    return -1;
}