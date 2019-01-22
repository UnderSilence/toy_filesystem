#pragma once
#include "structs.h"

enum ERROR_CODE {
	NO_ERROR,
	MKDIR_REPEAT_NAME,
	MKDIR_TOO_LONG,
	MKDIR_ILLEGAL_NAME,
	TOUCH_REPEAT_NAME,
	TOUCH_TOO_LONG,
	TOUCH_ILLEGAL_NAME,
	CREATE_INODE_FAIL,
	CREATE_BLOCK_FAIL,
	LIST_SUCCESS,
	CD_PATH_NOT_FOUND,
	RM_PATH_NOT_FOUND,
	CP_PATH_NOT_FOUND,
	RD_PATH_NOT_FOUND,
	WR_PATH_NOT_FOUND,
	PATH_NOT_FOUND
};

class Filesystem{

private:
	inode root_inode;
	std::vector<dentry> root_dentries;

	super_block sb;
	std::vector<uint32> free_block;
	std::vector<uint32> free_inode;
	uint8 inode_bitmap[INODE_BITMAP / 8];
	uint8 block_bitmap[BLOCK_BITMAP / 8];


	/*�û�������Ա*/

	inode cur_inode;
	std::vector<dentry> cur_dentries; //���ڱ�����Ŀ¼
	dentry cur_dentry; //���ڱ��浱ǰ�ļ�����
	char cur_path[256]; //��ǰ����·��

public:
	Filesystem() {
		init_filesystem();
	}

	timespec get_current_time();
	void read_inode(inode* self);
	void write_inode(inode* self);
 
	uint32 create_inode();	//�����������������
	void release_inode(uint32 inode_offset);
	uint32 create_block();	//����������¿��
	void release_block(uint32 block_offset);

	uint32 search_block(const inode* self, uint32 index); //����Ѱ��inode�ڵ�index���������λ�ã�����Ѱַ����

	//ϵͳ��������
	void exit();
	int terminal(int argc, char argv[][128]); //���ն˽����û�����

	//��ʼ�������б�
	void init_root(); //��һ�ν�����̣������ļ�ϵͳ
	void init_filesystem();

	//Ŀ¼��������
	ERROR_CODE touch(inode* self,char* file_name); //�ڵ�ǰĿ¼����һ���ļ�
	ERROR_CODE mkdir(inode* self,const char* path_str); //�ڵ�ǰĿ¼����һ���ļ���,��������
	ERROR_CODE cd(inode* self,char* path_str);
	ERROR_CODE cp(inode* self,char* src_path, char* dst_path);
	ERROR_CODE rm(inode * self, char * path_str);
	ERROR_CODE ls(inode* self, uint32 mode);
	ERROR_CODE write(inode* self, char* file_name, char* buffer);
	ERROR_CODE read(inode* self, char* file_name);
	void help(char* details = nullptr);

	//�ļ���������
	void read_block(uint32 block_offset, uint32 byte_offset,void* buffer, uint32 size);
	void write_block(uint32 block_offset, uint32 byte_offset,void* buffer, uint32 size);

	void read_dentry(inode* self, std::vector<dentry>& dentries);
	void write_dentry(inode* self, std::vector<dentry>& dentries);
	//static bool check_legal_str(const char* str);

	void find_full_path(inode* self, char* path_str);

	void copy_file(inode * self, inode * src, char * path_str);
	void copy_directory(inode * self, inode * src, char * path_str);

	ERROR_CODE rn(inode * self, char * old_name, char * new_name);

	//һЩ��̬����
	static bool check_legal_name(const char *ptr);
	static uint32 FUNCTION_COUNT;
};
