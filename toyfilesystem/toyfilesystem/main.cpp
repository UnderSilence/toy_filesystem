#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include "structs.h"
#include "filesystem.h"


void debug() {
	dentry dentries[16];
	std::cout <<"dentry:"<< sizeof(dentry) << std::endl;
	std::cout << "dnode:" << sizeof(dentries) << std::endl;
}
void clear_disk() {
	FILE *disk = fopen("disk", "w+");
	for (int i = 0; i<200480; i++)//20480=2Mb,днЪБ2mb
	fprintf(disk, "%c", '\0');
	fclose(disk);
}
int main() {
	//debug();
	//clear_disk();
	Filesystem fs;
	
	int argc = 0, length;
	char argv[8][128] = {0};
	char buf[128] = {0};

	do {
		length = strlen(buf);
		if (length) buf[length - 1] = 0;
		char* temp = strtok(buf, " ");
		while (temp) {
			strcpy(argv[argc++], temp);
			temp = strtok(NULL, " ");
		}
		if (fs.terminal(argc, argv)) {
			break;
		}
		argc = 0;
	} while (fgets(buf, sizeof(buf), stdin));


	system("pause");
	return 0;
}