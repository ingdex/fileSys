//
// Created by ingdex on 19-2-25.
// inode[0]为主目录索引节点
// 最多支持32个用户，每个用户用id唯一标识，第i个用户的id第i位bit为1，其余位为0
// 目录文件的每一块用开始4字节记录本块存储的目录项个数
// 普通文件的每一块用开始4字节记录存放的字节数
#include <iostream>
#include <fstream>
#include <time.h>
#include <stdio.h>

using namespace std;

#ifndef FILESYS_FILESYS_H
#define FILESYS_FILESYS_H


#define BLOCK_SIEZ 4096      //块大小为4KBinode
#define USR_REGION_OFFSET 255   //用户名密码存储区在管理块中的偏移
#define S_ISIZE 20          //i结点区总块数
#define S_FSIZE 4096        //文件卷总块数, 4096 * 512B = 2MB
#define S_NFREE 490         //空闲块数
#define S_NINDOE 128        //空闲i结点数
#define FIRST_INODE_BLOCK 2 //第一块inode块位置
#define INODE_SIZE 256
#define ENTRY_SIZE 512
#define FIRST_FREE_BLOCK 22   //第一块空闲块位置
#define MAX_ENTRY_COUNT 15  //目录文件每一块可存放最大目录项数
#define MAX_CHAR_COUNT 508
#define BIT_MAP 1       //空闲块位示图存放块下标
#define DIR 0   //目录文件
#define REG_FILE 1  //普通文件
#define S_FLAG 0xfcfcfc   //标识文件卷是否是第一次挂载

//引导块，暂时无用
struct BootBlock{

};

//管理块
struct ManagementBlock{
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
};

struct InodeBitMap{   //inode位示图
    char byte[S_NINDOE / 8];
};

struct BitMap{   //空闲块位示图
    char byte[BLOCK_SIEZ];
};

struct Inode{   //inode
    unsigned int i_uid, i_gid;  //用户权限
    unsigned int i_type;        //文件类型，：0目录，1：普通文件
    unsigned int i_mode;        //文件存取权限
    unsigned int i_ilink;       //联结计数
    unsigned int i_addr[13];    //地址索引表，实际只使用2级索引
    unsigned long i_time;       //文件存取时间
    unsigned long i_atime;      //最后访问时间
    unsigned long i_mtime;      //最后修改(modify)时间
    unsigned long i_ctime;      //最后改变(change)时间
    unsigned int i_blkbits;     //以位为单位的块大小
    unsigned long i_blksize;    //以字节为单位的块大小
    unsigned long long i_size;           //以字节为单位的文件大小
    unsigned long i_blocks;     //文件的块数
    unsigned short i_bytes;     //最后一块使用的字节数
};

struct Usr{
    char name[20];
    int id;
    char password[20];
};

#define MY_NAME_LEN 256

//为了对齐，一目录项在一个数据块上占用512字节，一个4k数据块最多存储8个目录项
struct DirEntry{
    uint32_t    inode;			    /* Inode number */
    uint16_t    rec_len;		    /* Directory entry length */
    uint8_t	    name_len;		    /* Name length */
    uint8_t	    file_type;
    char	    name[MY_NAME_LEN];	/* File name */
};

//打开文件结构，记录文件的基本信息、读取或者写入位置
struct FileHandle{
    Inode inode;
    unsigned int inodeNum;
    int index;//标志当前读写到了第几个索引块，0-9表示读写到了直接索引块，10、11、12分别表示读到了一级、二级、三级索引，index=1代表下一个要读写的块是9
    unsigned int pos;   //表示当前读写到的数据块的偏移，取值范围0-4096
    unsigned int index0pos;//表示读写到当前一级索引的第几个直接索引，取值范围0-1024
    unsigned int index1pos;//表示读写到当前二级索引的第几个一级索引，取值范围0-1024
    unsigned int index2pos;//表示读写到当前三级索引的第几个二级索引，取值范围0-1024
    unsigned long long blocks;  //当前引已经读写的块数目
};

class FileSys{
private:
    BootBlock bootBlock;
    ManagementBlock managementBlock;
    unsigned int inodeBeg;  //inode区的第一块编号
//    InodeBitMap inodeBitMap;
//    BitMap bitmap;
    //Inode inode;
    string volumeName;
    int curUid;
    Usr usr;
    int logined;
    int usrCount;
    int curOffset;
    unsigned int mallocInode();              //申请一个空闲inode，返回inode结点相对位置，不存在空闲inode时返回-1
    unsigned int mallocBlock();              //申请一个空闲块，返回空闲块绝对块号，不存在空闲块时返回-1
    int freeInode(int inodePos);    //释放inode
    int freeBlock(int blockPos);    //释放block
    Inode getInode(int inodeNum);  //获取inode结点
    unsigned int getDataBlocksCount(unsigned long i_blocks);//获取实际存放数据的块数
    int writeInode(Inode inode, int inodeNum);
    int updateManegementBlock();
    int rmDir(int inodeNum);
    int rmRegFile(int inodeNum);
    unsigned int findFile(unsigned int parentInodeNum, string filename);
    bool findFileInEntryBlk(DirEntry entrys[1024], string filename);
    void getEntryFromBlk(DirEntry entry[], unsigned int addr);
    unsigned int findEntryBy0Index(unsigned int addr, string dirName);
    int writeBlk(FileHandle &handle, char *buf, unsigned int size);//以块为单位写普通文件，除了最后一块之外默认其余块都是一整块
    int readBlk(FileHandle &handle, char *buf);//以块为单位读取文本文件，返回值为读取到的数据字节数
public:
    FileSys(string filename);
    ~FileSys();

    int init();                     //初始化文件系统
    int login();
    int createFile(string filename,char type, int uid, int parentInodeNum); //创建成功时返回新文件inode号，否则返回-1    //建立文件
    int deleteFile(int inodeNum);    //删除文件
    int openFile(Inode inode);      //打开文件
    int closeFile(Inode inode);     //关闭文件
    int readFile();                 //读文件
    int displayFile(int inodeNum);   //显示文件内容，如果是目录文件，显示目录项，如果是普通文件，显示文件内容
    int cdDir(string dirName, unsigned int &curdir);          //进入目录
    int catFile(unsigned int parentInodeNum, string filename);      //输出普通文件内容
    int writeFile(unsigned int parentInodeNum, string filename);    //写文件
    int rmFile(unsigned int parentInodeNum, string filename);       //删除文件

};

#endif //FILESYS_FILESYS_H
