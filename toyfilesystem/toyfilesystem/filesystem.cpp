#define _CRT_SECURE_NO_WARNINGS
#include "structs.h"
#include "filesystem.h"
#include <iostream>
#include <cassert>
#include <cstring>
#include <string>


uint32 Filesystem::FUNCTION_COUNT = 9;

char help_list[32][2][128] = {
	{"ls","to list current directory."},
	{"touch","touch \"file_name\" to create a file."},
	{"mkdir","touch \"dir_name\" to create a directory."},
	{"cd", "cd \"path\" move to other's directory."},
	{"rm", "rm \"filename\" to remove a file or directory(cascade)."},
	{"cp", "cp \"path\" \"filename\" to copy a file or directory(cascade)."},
	{"write", "write \"path\" \"string(128)\" to write bytes in file(over write)."},
	{"read", "read \"path\" to read bytes in file."},
	{"exit", "needless to say."}
};

void Filesystem::write_block(uint32 block_offset, uint32 byte_offset,void* buffer, uint32 size) {
	FILE *disk = fopen("disk", "rb+");
	fseek(disk, BLOCK_OFFSET+block_offset*sb.s_blocksize, 0);
	fwrite(buffer, size, 1, disk);
	fclose(disk);
}

void Filesystem::read_block(uint32 block_offset, uint32 byte_offset,void* buffer, uint32 size) {
	FILE *disk = fopen("disk", "rb+");
	fseek(disk, BLOCK_OFFSET + block_offset*sb.s_blocksize, 0);
	fread(buffer, size, 1, disk);
	fclose(disk);
}

void Filesystem::read_inode(inode* self) {
	FILE *disk = fopen("disk", "rb+");
	fseek(disk, INODE_OFFSET + self->i_ino*sb.s_inodesize, 0);
	fread(self, sizeof(inode), 1, disk);
	fclose(disk);
}

void Filesystem::write_inode(inode* self) {
	FILE *disk = fopen("disk", "rb+");
	fseek(disk, INODE_OFFSET + self->i_ino*sb.s_inodesize, 0);
	fwrite(self, sizeof(inode), 1, disk);
	fclose(disk);
}

uint32 Filesystem::create_block() {
	if (free_inode.empty()) {
		printf("there is no storage space for new block.");
		return -1;
	}
	uint32 block_offset = free_block.back();
	free_block.pop_back();
	block_bitmap[block_offset / 8] ^= (1 << (block_offset & 7));

	FILE *disk = fopen("disk", "rb+");
	fseek(disk, SUPER_BLOCK_SIZE + INODE_BITMAP / 8 + block_offset / 8, 0);
	fwrite(&block_bitmap[block_offset / 8], sizeof(uint8), 1, disk);
	fclose(disk);
	return block_offset;
}

void Filesystem::release_block(uint32 block_offset) {
	free_block.push_back(block_offset);
	block_bitmap[block_offset / 8] ^= (1 << (block_offset & 7));
	
	FILE *disk = fopen("disk", "rb+");
	fseek(disk, SUPER_BLOCK_SIZE + INODE_BITMAP / 8 + block_offset / 8, 0);
	fwrite(&block_bitmap[block_offset / 8], sizeof(uint8), 1, disk);
	fclose(disk);
}

uint32 Filesystem::create_inode() {
	if (free_inode.empty()) {
		printf("there is no storage space for new inode.");
		return -1;
	}
	uint32 inode_offset = free_inode.back();
	free_inode.pop_back();
	inode_bitmap[inode_offset / 8] ^= (1 << (inode_offset & 7));
	
	FILE* disk = fopen("disk", "rb+");
	fseek(disk,SUPER_BLOCK_SIZE+ inode_offset /8,0);
	fwrite(&inode_bitmap[inode_offset / 8], sizeof(uint8), 1, disk);
	fclose(disk);
	return inode_offset;
}

void Filesystem::release_inode(uint32 inode_offset) {
	free_inode.push_back(inode_offset);
	inode_bitmap[inode_offset / 8] ^= (1 << (inode_offset & 7));
	
	inode cur_inode;
	cur_inode.i_ino = inode_offset;
	read_inode(&cur_inode);
	for (int i = 0; i < cur_inode.i_blocks; i++) {
		release_block(search_block(&cur_inode, i));
	}

	FILE* disk = fopen("disk", "rb+");
	fseek(disk, SUPER_BLOCK_SIZE + inode_offset / 8, 0);
	fwrite(&inode_bitmap[inode_offset / 8], sizeof(uint8), 1, disk);
	fclose(disk);
}

uint32 Filesystem::search_block(const inode* self, uint32 index) { //抽象了寻址过程
	if (index > self->i_blocks) {
		printf("file or directory read error(over boundary).");
		return -1;
	}
	if (index < 8) {
		return self->i_block[index];
	}
	else if(index < 8 + 128){
		pointer_block pb;
		read_block(self->i_block1, 0, &pb, sizeof(pb));
		return pb.i_block[index - 8];
	}
	else { //二级索引 annoying..

	}
}

void Filesystem::read_dentry(inode* self, std::vector<dentry>& dentries) {
	dentries.clear(); 
	//清空一下缓冲，再将目录从磁盘取出
	uint32 blocks_count = self->i_bytes/sizeof(dentry);
	dentry dentry_buf[DIR_PER_BLOCK];
	//每16个目录项取一次
	for (int i = 0; blocks_count && i < self->i_blocks; i++) {
		if (i < 4) {
			read_block(self->i_block[i], 0, dentry_buf, sizeof(dentry_buf));
		}
		for (int j = 0; blocks_count && j < DIR_PER_BLOCK;j++, blocks_count--) {
			dentries.push_back(dentry_buf[j]);
		}
	}
}

void Filesystem::write_dentry(inode* self, std::vector<dentry>& dentries) {
	//暂不考虑一级索引
	while (self->i_blocks > (dentries.size() + DIR_PER_BLOCK - 1) / DIR_PER_BLOCK) { //原来块数比较多
		release_block(self->i_block[--self->i_blocks]);
	}
	while (self->i_blocks < (dentries.size() + DIR_PER_BLOCK - 1) / DIR_PER_BLOCK) { //原来块数比较少
		self->i_block[self->i_block[self->i_blocks++]] = create_block();
	}

	self->i_bytes = dentries.size() * sizeof(dentry);
	self->i_blocks = (dentries.size() + DIR_PER_BLOCK - 1) / DIR_PER_BLOCK;
	uint32 blocks_count = dentries.size();
	dentry dentry_buf[DIR_PER_BLOCK] = {0};

	for (int i = 0; blocks_count && i < self->i_blocks; i++) {
		for (int j = 0; blocks_count && j < DIR_PER_BLOCK; j++, blocks_count--) { //每16个1存
			dentry_buf[j] = dentries[dentries.size() - blocks_count];
		}
		write_block(search_block(self, i), 0, dentry_buf, sizeof(dentry_buf));
	}
}

void Filesystem::init_root() {
	root_inode.i_ino = create_inode();
	root_inode.i_type = 1;  //代表其为一目录文件
	
	std::vector<dentry> dentries{ {".",root_inode.i_ino},{"..",root_inode.i_ino} };
	write_dentry(&root_inode, dentries);
	write_inode(&root_inode);
}

void Filesystem::init_filesystem() {
	FILE *disk = fopen("disk", "rb+");

	fread(&sb, sizeof(super_block), 1, disk);
	sb.s_blockcount = BLOCK_BITMAP;
	sb.s_inodecount = INODE_BITMAP;
	sb.s_blocksize = BLOCK_SIZE;
	sb.s_inodesize = INODE_SIZE;

	fseek(disk, SUPER_BLOCK_SIZE, 0);
	fread(&inode_bitmap, sizeof(inode_bitmap), 1, disk);
	fread(&block_bitmap, sizeof(block_bitmap), 1, disk);
	fseek(disk, INODE_OFFSET, 0);
	fread(&root_inode, sizeof(inode), 1, disk);

	fclose(disk);
	/*用一个vector来维护空闲块, bitmap置1代表块被占用*/
	for (int i = 0; i < sb.s_inodecount / 8; i++) {
		for (int j = 0; j < 8; j++) {
			if (!(inode_bitmap[i] >> j & 1)) free_inode.push_back(i * 8 + j);
		}
	}
	for (int i = 0; i < sb.s_blockcount / 8; i++) {
		for (int j = 0; j < 8; j++) {
			if (!(block_bitmap[i] >> j & 1)) free_block.push_back(i * 8 + j);
		}
	}
	std::reverse(free_inode.begin(), free_inode.end());
	std::reverse(free_block.begin(), free_block.end());

	if ((inode_bitmap[0] & 1) == 0) { //0号inode 为空，及没有主目录
		init_root();
	}

	strcpy(cur_path, "/"); //将初始路径设置为'/'
	cur_inode = root_inode;
}

ERROR_CODE Filesystem::mkdir(inode* self,const char* path_str) {
	std::vector<dentry> dentries;
	read_dentry(self, dentries);
	
	if (strlen(path_str) > MAX_NAME_LEN-1) {
		printf("mkdir: can not create file '%s': File name length exceeded.\n");
		return MKDIR_TOO_LONG;
	}
	if (!check_legal_name(path_str)) {
		printf("mkdir: file or directory name can't not contains '/'.");
		return MKDIR_ILLEGAL_NAME;
	}
	for (dentry& dir: dentries) {
		if (!strcmp(dir.d_name, path_str)) {
			printf("mkdir: can not create file '%s': File exists\n", path_str);
			return MKDIR_REPEAT_NAME;
		}
	}

	inode new_inode = {0};
	dentry new_dentry = {0};
	new_inode.i_ino = new_dentry.d_inode = create_inode();	
	
	//优先跟新父节点信息
	strcpy(new_dentry.d_name, path_str);
	dentries.push_back(new_dentry);
	write_dentry(self, dentries);
	write_inode(self);

	//指向父目录和本身的目录项在创建文件夹的时候就建立好
	new_inode.i_type = 1;
	dentries.clear();
	dentries.push_back({ ".", new_dentry.d_inode }); 
	dentries.push_back({ "..", self->i_ino });
	write_dentry(&new_inode, dentries);
	write_inode(&new_inode);
	return NO_ERROR;
}

ERROR_CODE Filesystem::touch(inode* self, char* path_str) {
	std::vector<dentry> dentries;
	read_dentry(self, dentries);

	if (strlen(path_str) > MAX_NAME_LEN - 1) {
		printf("touch: can not create file '%s': File name length exceeded\n");
		return TOUCH_TOO_LONG;
	}
	if (!check_legal_name(path_str)) {
		printf("touch: file or directory name can't not contains '/'");
		return TOUCH_ILLEGAL_NAME;
	}
	for (dentry& dir : dentries) {
		if (!strcmp(dir.d_name, path_str)) {
			printf("touch: can not create file '%s': File exists\n", path_str);
			return TOUCH_REPEAT_NAME;
		}
	}
	inode new_inode = { 0 };
	dentry new_dentry = { 0 };
	new_inode.i_ino = new_dentry.d_inode = create_inode();

	//优先跟新父节点信息
	strcpy(new_dentry.d_name, path_str);
	dentries.push_back(new_dentry);
	write_dentry(self, dentries);
	write_inode(self);

	//指向父目录和本身的目录项在创建文件夹的时候就建立好
	new_inode.i_type = 0;
	dentries.clear();
	write_inode(&new_inode);
	return NO_ERROR;
}


ERROR_CODE Filesystem::ls(inode* self, uint32 mode) {
	std::vector<dentry> dentries;
	read_dentry(self, dentries);
	if(!mode){
		printf("\tfile\tinode\n");
		for (dentry& d : dentries) {
			printf("\t%s\t%u\n", d.d_name, d.d_inode);
		}
	}
	else if (mode&1) {
		inode temp;
		printf("\tfile\tinode\ttype\tblocks\tbytes\n");
		for (dentry& d : dentries) {
			temp.i_ino = d.d_inode;
			read_inode(&temp);
			printf("\t%s\t%u\t%u\t%u\t%u\n", d.d_name, d.d_inode, temp.i_type, temp.i_blocks, temp.i_bytes);
		}
	}
	return NO_ERROR;
}

ERROR_CODE Filesystem::cd(inode* self, char* path_str) {
	
	char d_name[MAX_NAME_LEN] = {0};

	inode nxt_inode = *self;
	std::vector<dentry> dentries;

	bool abs_path = false;
	if (path_str[0] == '/') {
		nxt_inode = root_inode;
		path_str++;
	}
	char *temp = strtok(path_str, "/");
	while (temp) {
		strcpy(d_name, temp);
		bool flag = false;
		read_dentry(&nxt_inode, dentries);
		for (dentry d : dentries) {
			if (!strcmp(d.d_name, d_name)) { //找到匹配项
				nxt_inode.i_ino = d.d_inode;
				read_inode(&nxt_inode);
				flag = true;
				break;
			}
		}
		if (!flag) {
			printf("cd: Path doesn't exists");
			return CD_PATH_NOT_FOUND;
		}
		temp = strtok(NULL, "/");
	}

	if (nxt_inode.i_type == 0) {
		printf("cd: Can not cd into a normal file");
		return CD_PATH_NOT_FOUND;
	}

	*self = nxt_inode;
	find_full_path(self, cur_path);
	return NO_ERROR;
}

void Filesystem::find_full_path(inode* self, char* path_str) {
	strcpy(path_str, ""); //清空路径
	inode cur_inode = *self, prev_inode;
	std::vector<dentry> dentries; //目录项缓冲区

	if (cur_inode.i_ino == root_inode.i_ino) {
		strcpy(path_str, "/");
		return;
	}

	while (cur_inode.i_ino != root_inode.i_ino) {
		read_dentry(&cur_inode, dentries);
		for (dentry& d : dentries) {
			if (!strcmp(d.d_name, "..")) {
				prev_inode.i_ino = d.d_inode;
				read_inode(&prev_inode);
				break;
			}
		}
		read_dentry(&prev_inode, dentries);
		for (dentry& d : dentries) {
			if (d.d_inode == cur_inode.i_ino) {
				strcat(path_str, d.d_name);
				strcat(path_str, "/");
			}
		}
		cur_inode = prev_inode;
	}
	_strrev(path_str);
}

void Filesystem::copy_file(inode *self, inode * src, char * path_str) {
	std::vector<dentry> dentries;
	read_dentry(self, dentries);
	inode file_inode;
	for (dentry& d : dentries) {
		if (!strcmp(d.d_name, path_str)) {
			file_inode.i_ino = d.d_inode;
			read_inode(&file_inode);
			break;
		}
	}
	for (int i = 0; i < file_inode.i_blocks; i++) {
		release_block(search_block(&file_inode, i));
	} //安全保险起见，释放一次，以防数据块浪费

	file_inode.i_blocks = src->i_blocks;
	file_inode.i_bytes = src->i_bytes;

	char buffer[BLOCK_SIZE];

	for (int i = 0; i < file_inode.i_blocks; i++) {
		file_inode.i_block[i] = create_block();
		read_block(search_block(src, i), 0, buffer, BLOCK_SIZE);
		write_block(search_block(&file_inode, i), 0, buffer, BLOCK_SIZE);
	}
	write_inode(&file_inode);
}

void Filesystem::copy_directory(inode *self, inode * src, char * path_str) {
	std::vector<dentry> dentries;
	read_dentry(self, dentries);
	inode directory_inode;

	for (dentry& d : dentries) {
		if (!strcmp(d.d_name, path_str)) {
			directory_inode.i_ino = d.d_inode;
			read_inode(&directory_inode);
		}
	}
	read_dentry(src, dentries);
	for (dentry& d : dentries) {
		if (!strcmp(d.d_name, ".")) continue;
		if (!strcmp(d.d_name, "..")) continue;

		inode nxt_inode;
		nxt_inode.i_ino = d.d_inode;
		read_inode(&nxt_inode);

		if (nxt_inode.i_type == 0) {
			touch(&directory_inode, d.d_name);
			copy_file(&directory_inode, &nxt_inode, d.d_name);
		}
		else if (nxt_inode.i_type == 1) {
			mkdir(&directory_inode, d.d_name);
			copy_directory(&directory_inode, &nxt_inode, d.d_name);
		}
	}	
}
ERROR_CODE Filesystem::rn(inode* self, char* old_name, char* new_name) {
	std::vector<dentry> dentries;
	read_dentry(self, dentries);

	for (dentry& d : dentries) {
		if (!strcmp(d.d_name, old_name)) {
			strcpy(d.d_name,new_name);
			write_dentry(self, dentries);
			return NO_ERROR;
		}
	}

	printf("rn: Path doesn't exists");
	return PATH_NOT_FOUND;
} 


ERROR_CODE Filesystem::cp(inode* self,char* path_str, char* dst_name) {
	std::vector<dentry> dentries;
	read_dentry(self, dentries);

	char d_name[MAX_NAME_LEN] = { 0 };

	inode nxt_inode = *self;

	bool abs_path = false;
	if (path_str[0] == '/') {
		nxt_inode = root_inode;
		path_str++;
	}
	char *temp = strtok(path_str, "/");
	while (temp) {
		strcpy(d_name, temp);
		bool flag = false;
		read_dentry(&nxt_inode, dentries);
		for (dentry d : dentries) {
			if (!strcmp(d.d_name, d_name)) { //找到匹配项
				nxt_inode.i_ino = d.d_inode;
				read_inode(&nxt_inode);
				flag = true;
				break;
			}
		}
		if (!flag) {
			printf("cp: Path doesn't exists");
			return CD_PATH_NOT_FOUND;
		}
		temp = strtok(NULL, "/");
	}

	if (nxt_inode.i_type == 0) { //文件那么直接拷贝
		if (touch(self, dst_name) != NO_ERROR) {
			printf("cp: Directory already exists");
			return PATH_NOT_FOUND;
		}
		copy_file(self, &nxt_inode, dst_name);
	}
	else if (nxt_inode.i_type == 1) {
		if (mkdir(self, dst_name) != NO_ERROR) {
			printf("cp: Directory already exists");
			return PATH_NOT_FOUND;
		}
		copy_directory(self, &nxt_inode, dst_name);
	}
	return NO_ERROR;
}

ERROR_CODE Filesystem::rm(inode* self, char* path_str) {
	if (!strcmp("..", path_str) || !strcmp(".", path_str)) {
		printf("rm: Illegal operations");
		return PATH_NOT_FOUND;
	}
	std::vector<dentry> dentries;
	inode nxt_inode;
	read_dentry(self, dentries);
	for (auto it = dentries.begin(); it != dentries.end();it++) {
		if (!strcmp(it->d_name, path_str)) {
			nxt_inode.i_ino = it->d_inode;
			read_inode(&nxt_inode);

			dentries.erase(it);
			write_dentry(self, dentries);

			if (nxt_inode.i_type == 0) {
				release_inode(nxt_inode.i_ino);
			}
			else {
				read_dentry(&nxt_inode, dentries);
				for (dentry& d : dentries) {
					if (!strcmp(d.d_name, ".")) continue;
					if (!strcmp(d.d_name, "..")) continue;
					rm(&nxt_inode, d.d_name);
				}
				
				release_inode(nxt_inode.i_ino);
			}
			return NO_ERROR;
		}
	}
	printf("rm: Can not find the path\n");
	return PATH_NOT_FOUND;
}

ERROR_CODE Filesystem::write(inode* self, char* path_str, char* buffer) {
	char d_name[MAX_NAME_LEN];

	inode nxt_inode = *self;
	std::vector<dentry> dentries;

	bool abs_path = false;
	if (path_str[0] == '/') {
		nxt_inode = root_inode;
		path_str++;
	}
	char *temp = strtok(path_str, "/");
	while (temp) {
		strcpy(d_name, temp);
		bool flag = false;
		read_dentry(&nxt_inode, dentries);
		for (dentry d : dentries) {
			if (!strcmp(d.d_name, d_name)) { //找到匹配项
				nxt_inode.i_ino = d.d_inode;
				read_inode(&nxt_inode);
				flag = true;
				break;
			}
		}
		if (!flag) {
			printf("write: Path doesn't exists");
			return WR_PATH_NOT_FOUND;
		}
		temp = strtok(NULL, "/");
	}
	if (nxt_inode.i_type != 0) {
		printf("write: Only work on normal file");
		return PATH_NOT_FOUND;
	}

	for (int i = 0; i < nxt_inode.i_blocks; i++) {
		release_block(search_block(&nxt_inode, i));
	}

	uint32 length = strlen(buffer);
	nxt_inode.i_blocks = (length + BLOCK_SIZE - 1) / BLOCK_SIZE;
	nxt_inode.i_bytes = length;
	for (int i = 0; i < nxt_inode.i_blocks; i++) {
		nxt_inode.i_block[i] = create_block();
		if (strlen(buffer) > BLOCK_SIZE) {
			write_block(nxt_inode.i_block[i], 0, buffer, BLOCK_SIZE);
		} write_block(nxt_inode.i_block[i], 0, buffer, strlen(buffer));
		buffer += BLOCK_SIZE;
	}
	write_inode(&nxt_inode);
}

ERROR_CODE Filesystem::read(inode* self, char* path_str) {
	char d_name[MAX_NAME_LEN];

	inode nxt_inode = *self;
	std::vector<dentry> dentries;

	bool abs_path = false;
	if (path_str[0] == '/') {
		nxt_inode = root_inode;
		path_str++;
	}
	char *temp = strtok(path_str, "/");
	while (temp) {
		strcpy(d_name, temp);
		bool flag = false;
		read_dentry(&nxt_inode, dentries);
		for (dentry d : dentries) {
			if (!strcmp(d.d_name, d_name)) { //找到匹配项
				nxt_inode.i_ino = d.d_inode;
				read_inode(&nxt_inode);
				flag = true;
				break;
			}
		}
		if (!flag) {
			printf("read: Path doesn't exists");
			return RD_PATH_NOT_FOUND;
		}
		temp = strtok(NULL, "/");
	}
	if (nxt_inode.i_type == 1) {
		printf("read: Only work on normal file");
		return RD_PATH_NOT_FOUND;
	}
	uint8 buffer[BLOCK_SIZE];
	for (int i = 0; i < nxt_inode.i_blocks; i++) {
		read_block(search_block(&nxt_inode, i), 0, buffer, sizeof(buffer));
		for (int i = 0, index = 0; i < 16; i++) {
			for (int j = 0; j < 16; j++) {
				printf(" %c%c ", buffer[index], buffer[index+1]);
				index += 2;
			}
			puts("");
		}
	}
}

void Filesystem::help(char * details)
{
	if (details == nullptr) {
		printf("command list(+? [xx] for details)\n");
		for (int i = 0; i < Filesystem::FUNCTION_COUNT; i++) {
			if (i) printf(", ");
			printf("%s", help_list[i][0]);
		}
	}
	else {
		for (int i = 0; i < Filesystem::FUNCTION_COUNT; i++) {
			if (!strcmp(details, help_list[i][0])) {
				printf("#%s\n%s\n", help_list[i][0], help_list[i][1]);
			}
		}
	}
}

void Filesystem::exit() {
	printf("fs : System stop, see u next time\n");
}


int Filesystem::terminal(int argc, char argv[][128]) {

	if (argc == 0) {
		printf("\nroot@undersilence:%s#", cur_path);
		return 0;
	}
	else {
		if (!strcmp(argv[0],"ls")) {
			if(argc>0&&!strcmp(argv[1],"-a")) ls(&cur_inode, 1);
			else ls(&cur_inode, 0);
		}
		else if (!strcmp(argv[0], "mkdir")) {
			if (argc < 2) {
				printf("%s: Parameter not found\n", argv[0]);
			} else mkdir(&cur_inode, argv[1]);
		}
		else if (!strcmp(argv[0], "touch")) {
			if (argc < 2) {
				printf("%s: Parameter not found\n", argv[0]);
			} else touch(&cur_inode, argv[1]);
		}
		else if (!strcmp(argv[0], "cd")) {
			if (argc < 2) {
				printf("%s: Parameter not found\n", argv[0]);
			} else cd(&cur_inode, argv[1]);
		}
		else if (!strcmp(argv[0], "write")) {
			if (argc < 3) {
				printf("%s: Parameter not found\n", argv[0]);
			} else write(&cur_inode, argv[1], argv[2]);
		}
		else if (!strcmp(argv[0], "read")) {
			if (argc < 2) {
				printf("%s: Parameter not found\n", argv[0]);
			} else read(&cur_inode, argv[1]);
		}
		else if (!strcmp(argv[0], "cp")) {
			if (argc < 3) {
				printf("%s: Parameter not found\n", argv[0]);
			} else cp(&cur_inode, argv[1], argv[2]);
		}
		else if (!strcmp(argv[0], "rm")) {
			if (argc < 2) {
				printf("%s: Parameter not found\n", argv[0]);
			} else rm(&cur_inode, argv[1]);
		}
		else if (!strcmp(argv[0], "rn")) {
			if (argc < 3) {
				printf("%s: Parameter not found\n", argv[0]);
			} else rn(&cur_inode, argv[1], argv[2]);
		}
		else if (!strcmp(argv[0], "exit")) {
			exit();
			return 1;
		}
		else if (!strcmp(argv[0], "help")) {
			if (argc >= 3 && !strcmp("?",argv[1])) {
				help(argv[2]);
			}
			else help();
		}
		else {
			printf("%s: Command not found\n", argv[0]);
		}
		printf("\nroot@undersilence:%s#", cur_path);
	}
	return 0;
}
bool Filesystem::check_legal_name(const char *ptr) {
	while (*ptr) {
		if (*ptr == '/') return false;
		ptr++;
	}
	return true;
}
