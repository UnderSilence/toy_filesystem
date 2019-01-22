
/*
title��		structs.h
author:		undersilence
email:		undersilence@qq.com
details:
	����ļ�ϵͳ����Ҫ��һЩ���ݽṹ
*/

#pragma once
#include <vector>
#include <list>

typedef unsigned char uint8;
typedef unsigned int uint16;
typedef unsigned long uint32;

//����Ӧ�ô�����supperblock��
const uint32 SUPER_BLOCK_SIZE = 64;
const uint32 INODE_BITMAP = 64;
const uint32 BLOCK_BITMAP = 256; //���ݿռ� bit����һ����

const uint32 INODE_OFFSET = SUPER_BLOCK_SIZE+INODE_BITMAP/8+BLOCK_BITMAP/8; //֮����inode��
const uint32 INODE_SIZE = 64;

const uint32 BLOCK_OFFSET = INODE_OFFSET+INODE_SIZE*INODE_BITMAP; //֮�������ݿ�
const uint32 BLOCK_SIZE = 512; 
const uint32 MAX_NAME_LEN = 28;

const uint32 DIR_PER_BLOCK = 16;

typedef char nstr[MAX_NAME_LEN];

struct timespec {
	uint32 seconds;
};

struct super_block {
	uint32 s_blockcount; //���ݿ�����
	uint32 s_inodecount; //inode�ڵ�����
	uint32 s_blocksize;
	uint32 s_inodesize;
	//uint32 s_maxbytes;
};

struct pointer_block {
	uint32 i_block[BLOCK_SIZE/sizeof(uint32)];
}; //���Ѱַ��

struct inode {
	uint32 i_ino;			//inode id�ţ�����Ѱַ
	//uint32 i_count;			//count���Ӽ���
	uint32 i_type;

	//uint32 i_writecount	//���߼��������߳�
	//uint32 i_readcount	//д�߼���
	//uint32 i_mode;		//����Ȩ�޿��ƣ�����û���û�ϵͳ��û��ʵ�֡� 
	//uint32 i_uid;
	//uint32 i_gid;

	//timespec atime;  //accesstime
	//timespec ctime;	//createtime
	//timespec rtime;	//readtime

	uint32 i_blocks;		//�ļ�����ͳ��
	uint32 i_bytes;			//�ļ����ֽ��� 
	uint32 i_block[8];		//ֱ�ӿ� 8*4bytes
	uint32 i_block1;		//һ����ӿ�
	//uint32 i_block2;		//������ӿ�

	//uint32 i_extendsize;
	//uint32 i_extend[16];
};

struct dentry {
	nstr d_name;
	uint32 d_inode;
};
