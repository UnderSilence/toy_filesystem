
/*
title：		structs.h
author:		undersilence
email:		undersilence@qq.com
details:
	玩具文件系统中需要的一些数据结构
*/

#pragma once
#include <vector>
#include <list>

typedef unsigned char uint8;
typedef unsigned int uint16;
typedef unsigned long uint32;

//本来应该储存在supperblock中
const uint32 SUPER_BLOCK_SIZE = 64;
const uint32 INODE_BITMAP = 64;
const uint32 BLOCK_BITMAP = 256; //数据空间 bit代表一个块

const uint32 INODE_OFFSET = SUPER_BLOCK_SIZE+INODE_BITMAP/8+BLOCK_BITMAP/8; //之后是inode块
const uint32 INODE_SIZE = 64;

const uint32 BLOCK_OFFSET = INODE_OFFSET+INODE_SIZE*INODE_BITMAP; //之后是数据块
const uint32 BLOCK_SIZE = 512; 
const uint32 MAX_NAME_LEN = 28;

const uint32 DIR_PER_BLOCK = 16;

typedef char nstr[MAX_NAME_LEN];

struct timespec {
	uint32 seconds;
};

struct super_block {
	uint32 s_blockcount; //数据块总数
	uint32 s_inodecount; //inode节点总数
	uint32 s_blocksize;
	uint32 s_inodesize;
	//uint32 s_maxbytes;
};

struct pointer_block {
	uint32 i_block[BLOCK_SIZE/sizeof(uint32)];
}; //间接寻址块

struct inode {
	uint32 i_ino;			//inode id号，用于寻址
	//uint32 i_count;			//count链接计数
	uint32 i_type;

	//uint32 i_writecount	//读者计数，多线程
	//uint32 i_readcount	//写者计数
	//uint32 i_mode;		//访问权限控制，由于没有用户系统，没有实现。 
	//uint32 i_uid;
	//uint32 i_gid;

	//timespec atime;  //accesstime
	//timespec ctime;	//createtime
	//timespec rtime;	//readtime

	uint32 i_blocks;		//文件块数统计
	uint32 i_bytes;			//文件的字节数 
	uint32 i_block[8];		//直接块 8*4bytes
	uint32 i_block1;		//一级间接块
	//uint32 i_block2;		//二级间接块

	//uint32 i_extendsize;
	//uint32 i_extend[16];
};

struct dentry {
	nstr d_name;
	uint32 d_inode;
};
