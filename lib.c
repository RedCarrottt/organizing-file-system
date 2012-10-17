#include <fuse.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "lib.h"

int ofs_check_path_len(const char *path) {	
	char tpath[PATH_MAX];
	char *p;
	
	if(strlen(path) > PATH_MAX) return -ENAMETOOLONG;		//전체 패스길이 체크
	
	strcpy(tpath, path);
	
	p = strtok(tpath+1, "/");
	
	while(p != NULL) {
		if(strlen(p) > NAME_MAX) return -ENAMETOOLONG;	//각각의 파일 길이 체크
		p = strtok(NULL,"/");
	}
	return 0;
}

int ofs_check_access(mode_t mode, uid_t uid, gid_t gid, int how) {
	int res=0;
	
	if (uid == fuse_get_context()->uid) {					// 오너의 권한 확인(1순위)
		if(how & R_OK) res |= (mode & S_IRUSR) ^ S_IRUSR;
		if(how & W_OK) res |= (mode & S_IWUSR) ^ S_IWUSR;
		if(how & X_OK) res |= (mode & S_IXUSR) ^ S_IXUSR;
	} else if (gid == fuse_get_context()->gid) {				// 그룹의 권한 확인(2순위)
		if(how & R_OK) res |= (mode & S_IRGRP) ^ S_IRGRP;
		if(how & W_OK) res |= (mode & S_IWGRP) ^ S_IWGRP;
		if(how & X_OK) res |= (mode & S_IXGRP) ^ S_IXGRP;
	} else {										// Other 권한 확인
		if(how & R_OK) res |= (mode & S_IROTH) ^ S_IROTH; 
		if(how & W_OK) res |= (mode & S_IWOTH) ^ S_IWOTH;
		if(how & X_OK) res |= (mode & S_IXOTH) ^ S_IXOTH;
	}
	
	if(res) 
		return -EACCES;
	else
		return 0;
}

char* ofs_parsingname(const char *path) {
	return strrchr(path,'/')+1; //경로의 마지막 이름만 리턴
}

char* ofs_parsingparent(const char *path) {
	char * parent;
	char * p;
	int size;
	p = strrchr(path, '/');
	if(p == NULL)
		return NULL;
	size = (int)(p - path);
	if(size == 0) {
		// 루트 디렉토리의 경우
		parent = (char *)malloc(sizeof(char)*1);
		strcpy(parent, "/");
	} else {
		parent = (char *)malloc(sizeof(char)*(size+2));
		strncpy(parent, path, size);
		*(parent + size) = '\0';
	}
	return parent;		// 부모 디렉토리의 경로 리턴
}

char* 	ofs_extension		(const char *path) {
	char* p;
	p = strrchr(path,'.')+1;
	if(p <= path + 1) {
		p = NULL;
	}
	return p;
}

char* 	ofs_typedirname	(const char *path) {
	char *extension;
	char *dirname;

	extension = ofs_extension(path);
	if(extension == NULL) {
		dirname = (char *)malloc(sizeof(char)*2);
		strcpy(dirname, "_");
	} else {
		dirname = (char *)malloc(sizeof(char)*(strlen(path)+2));
		strcpy(dirname, "_");
		strcat(dirname, extension);
	}
	return dirname;
}
