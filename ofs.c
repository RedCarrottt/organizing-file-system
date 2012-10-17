#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <limits.h>
#include <stdio.h>

#include "node.h"
#include "lib.h"

static ONODE *root;

static int ofs_chmod(const char *, mode_t); 
static int ofs_chown(const char *, uid_t, gid_t); 
static int ofs_truncate(const char *, off_t);
static int ofs_mknod(const char *, mode_t, dev_t); 
static int ofs_link(const char *, const char *); 
static int ofs_symlink(const char *, const char *); 
static int ofs_readlink(const char*, char *, size_t); 
static int ofs_getattr(const char *, struct stat *);
static int ofs_readdir(const char *, void *, fuse_fill_dir_t, off_t, struct fuse_file_info *);
static int ofs_access(const char *, int);
static int ofs_utime(const char *, struct utimbuf *); 
static int ofs_unlink(const char *);
static int ofs_rmdir(const char *);
static int ofs_mkdir(const char *, mode_t);
static int ofs_open(const char *, struct fuse_file_info *);
static int ofs_read(const char *, char *, size_t, off_t, struct fuse_file_info *);
static int ofs_write(const char *, const char *, size_t , off_t , struct fuse_file_info *); 
static int ofs_rename(const char *, const char *); 
static int ofs_opendir(const char *, struct fuse_file_info *);

int ofs_makenod (const char*, mode_t, dev_t);
void ofs_newtypedir(const char *);
void ofs_addtypelink(const char *);
int ofs_makedir(const char *, mode_t);
int ofs_unlink_node(const char *);
int ofs_removedir(const char *);
int ofs_rename_node(const char *, const char *);

static int ofs_chmod(const char *path, mode_t mode) 
{
	ONODE *node = NULL;
	ONODE *parent;
	
	/* 에러 체크 */
	if (ofs_check_path_len(path) != 0)					//파일 이름 적합성 검사
		return -ENAMETOOLONG;
	if((node = ofs_findnode(root, path)) == NULL)			//파일 존재 여부 검사 
		return -ENOENT;
	if(node -> of_stat -> of_uid != fuse_get_context()->uid && fuse_get_context()->uid != 0)	//Owner 혹은 Previliged User여부 확인
		return -EPERM;
	parent = ofs_findparent(root, path);			//변경할 노드의 상위 정보 구하기
	if(*(parent->name) == '_')		// 타입 디렉토리에서 타입 노드 변경 불가
		return -EACCES;
	
	/* 권한 변경 */
	node -> of_stat -> of_mode = mode; 					//파일의 Permmission 변경
	
	return 0;
}


static int ofs_chown(const char *path, uid_t uid, gid_t gid) 
{
	ONODE *node = NULL;
	ONODE *parent;
	
	/* 에러 체크  */
	if (ofs_check_path_len(path) != 0)							
		return -ENAMETOOLONG;
	if((node = ofs_findnode(root, path)) == NULL) 			
		return -ENOENT;
	if(node -> of_stat -> of_uid != fuse_get_context()->uid && fuse_get_context()->uid != 0)	//Owner 혹은 Previliged User여부 확인
		return -EPERM;
	parent = ofs_findparent(root, path);			//변경할 노드의 상위 정보 구하기
	if(*(parent->name) == '_')		// 타입 디렉토리에서 타입 노드 변경 불가
		return -EACCES;
	
	/* 소유권 변경 */
	node -> of_stat -> of_uid = uid;						//파일의 Owner 변경
	node -> of_stat -> of_gid = gid; 						//파일의 Group 변경

	return 0;
}


static int ofs_truncate(const char *path, off_t length) 
{
	ONODE *node = NULL;
	OSTAT *stat = NULL;
	ONODE *parent;
	
	/* 에러 체크 루틴 */
	if(length < 0)									//Truncate Offset이 음수일 경우 에러 반환
		return -EINVAL;				
	if (ofs_check_path_len(path) != 0) 					
		return -ENAMETOOLONG;
	if((node = ofs_findnode(root, path)) == NULL) 			
		return -ENOENT;
	parent = ofs_findparent(root, path);			//변경할 노드의 상위 정보 구하기
	if(*(parent->name) == '_')		// 타입 디렉토리에서 타입 노드 변경 불가
		return -EACCES;
		
	stat = node -> of_stat;
	if ((ofs_check_access(stat->of_mode, stat->of_uid, stat->of_gid, W_OK)) != 0 || 
		S_ISDIR(stat->of_mode)) 						//엑세스 권한및 디렉토리 여부 확인
			return -EACCES;

	/* 파일 길이 변경 */
	if(stat -> of_size > length) {							//파일 길이를 줄이는 경우
		node -> of_data = (byte_t*)realloc(node -> of_data, length);
		stat -> of_size = length;
	}											//파일 길이를 늘리는 경우 (아무것도 하지 않음)
		
	return 0;
}

// 파일 노드를 만든다.
int ofs_makenod (const char* path, mode_t mode, dev_t dev) {
	ONODE *newfile = NULL, *target;

	/* 에러 체크 */
	if (ofs_check_path_len(path) != 0) 
		return -ENAMETOOLONG;
	if (ofs_findnode(root, path) != NULL) 
		return -EEXIST;
	if((target = ofs_findparent(root, path)) == NULL) 		// 입력된 Path가 존재하는지 확인
		return -ENOENT;
	if(!S_ISDIR(target->of_stat->of_mode)) 				// Path가 디렉토리 인지 확인
		return -ENOTDIR;
	//파일이 생성될 디렉토리의 권한 확인
	if ((ofs_check_access(target->of_stat->of_mode, target->of_stat->of_uid, target->of_stat->of_gid, W_OK | X_OK)) != 0)
		return -EACCES;								

	/* Special Files 처리 */
	if (S_ISBLK(mode) || S_ISCHR(mode))
	{
		if(fuse_get_context()->uid != 0) return -EPERM;					// Previliged User 여부 확인
		newfile->of_stat->of_rdev = dev;
	}

	/* 파일 생성 - Real User ID와 Real Group ID를 얻어서 파일을 생성해 준다 */
	newfile = ofs_neONODE(ofs_parsingname(path), mode , fuse_get_context()->uid , fuse_get_context()->gid);
	ofs_insertnode(target, newfile);

	return 0;
}

void ofs_newtypedir(const char *path) {
	ofs_makedir(path, 0755);
}

void ofs_addtypelink(const char *path) {
	ONODE *onode, *typedir;
	char *typedir_name, *file_name;
	char *typedir_path, *parent_path, *link_path;
	char *old_path;

	// 부모 디렉토리로부터 타입 디렉토리를 찾는다.
	typedir_name = ofs_typedirname(path);
	onode = ofs_findnode(root, path);
	typedir = ofs_findnode(onode->parentdir, typedir_name);
	fprintf(stderr, "** typedir_name %s\n", typedir_name);

	// 부모 디렉토리와 타입 디렉토리의 경로를 얻는다.
	parent_path = ofs_parsingparent(path);
	typedir_path = (char *)malloc(sizeof(char)*(strlen(parent_path)+strlen(typedir_name)+1));
	strcpy(typedir_path, parent_path);
	strcat(typedir_path, typedir_name);
	fprintf(stderr, "** parent_path %s\n", parent_path);
	fprintf(stderr, "** typedir_path %s\n", typedir_path);

	// 타입 디렉토리가 없으면 새로 만든다.
	if(typedir == NULL)
		ofs_newtypedir(typedir_path);

	// 타입 디렉토리 내에 심볼릭 링크를 만든다.
	file_name = ofs_parsingname(path);
	old_path = (char *)malloc(sizeof(char)*(3+strlen(file_name)+1));
	strcpy(old_path, "../");
	strcat(old_path, file_name);
	fprintf(stderr, "** old_path %s\n", old_path);
	link_path = (char *)malloc(sizeof(char)*(strlen(typedir_path)+1+strlen(file_name)+1));
	strcpy(link_path, typedir_path);
	strcat(link_path, "/");
	strcat(link_path, file_name);
	fprintf(stderr, "** link_path %s\n", link_path);
	ofs_symlink(old_path, link_path);

	free(typedir_name);
	free(typedir_path);
	free(parent_path);
	free(link_path);
	free(old_path);
}

// 일반 파일 노드를 만든다.
static int ofs_mknod(const char *path, mode_t mode, dev_t dev) 
{
	int ret;
	char *node_name;
	ONODE *parent;

	/* 에러 체크 */
	if (ofs_check_path_len(path) != 0)
		return -ENAMETOOLONG;
	parent = ofs_findparent(root, path);			//삽입할 노드의 상위 정보 구하기
	if(*(parent->name) == '_')		// 타입 디렉토리에서는 생성 불가
		return -EACCES;
	node_name = ofs_parsingname(path);
	if(*node_name == '_')
		return -EINVAL;				// 파일 이름은 _로 시작할 수 없다.

	// 파일 노드를 만든다.
	ret = ofs_makenod(path, mode, dev);

	if(ret == 0) {
		fprintf(stderr, "** add type link %s\n", path);
		// 이 파일 노드에 대한 새로운 타입 링크를 추가한다.
		if(ofs_extension(node_name) != NULL) {	
			ofs_addtypelink(path);
		}
	}

	return ret;
}

static int ofs_link(const char *oldname, const char *newname) 
{
	ONODE *dstnode, *srcnode;
	int ret;
	
	/* 에러 체크 */
	if (ofs_check_path_len(oldname) != 0)
		return -ENAMETOOLONG;
	if((srcnode = ofs_findnode(root, oldname)) == NULL)
		return -ENOENT;
	if(S_ISDIR(srcnode->of_stat->of_mode) && fuse_get_context()->uid != 0)	//Hard Link생성 권한 확인
		return -EPERM;
	
	/* Hard Link 파일 생성 - 에러 발생시 에러 반환 */
	if((ret = ofs_makenod(newname, srcnode->of_stat->of_mode, srcnode->of_stat->of_rdev)) != 0) 
		return ret;
	
	/* 파일 연결 */
	srcnode -> of_stat -> of_nlink++;					//nlink 증가 시킴
	dstnode = ofs_findnode(root, newname);
	dstnode -> of_data = srcnode -> of_data;				//data정보 연결
	dstnode -> of_stat = srcnode -> of_stat;				//node정보 연결

	return 0;
}

static int ofs_symlink(const char *oldname, const char *newname) 
{
	ONODE *dstnode;
	int ret, len;
	
	/* 에러 체크 */
	if (ofs_check_path_len(oldname) != 0)
		return -ENAMETOOLONG;
		
	/* Symbolic Link 파일 생성 */
	if((ret = ofs_makenod(newname, S_IFLNK | 0777, 0)) != 0) 
		return ret;
	
	/* Symoblic Link 데이터 저장 */
	len = strlen(oldname)+1;
	dstnode = ofs_findnode(root, newname);
	ofs_setdata(dstnode, oldname, len, 0);					//파일의 데이터 영역에 이름을 저장
	
	return 0;
}

static int ofs_readlink(const char* path, char *buffer, size_t size) 
{
	ONODE *node = NULL;
	
	/* 에러 체크 */
	if (ofs_check_path_len(path) != 0) 
		return -ENAMETOOLONG;
	if ((node = ofs_findnode(root, path)) == NULL) 
		return -ENOENT;
	if ((ofs_check_access(node -> of_stat -> of_mode, node -> of_stat -> of_uid, node -> of_stat -> of_gid, R_OK)) != 0) 
		return -EACCES;
	if (!S_ISLNK(node-> of_stat ->of_mode)) 				//심볼릭 링크 파일인지 확인
		return -EINVAL;
	
	/* 심볼링 링크 저장 */
	memcpy(buffer, node->of_data, size); 
	
	return 0;
}

static int ofs_getattr(const char *path, struct stat *stbuf)
{
	ONODE *node = NULL;
	
	/* 에러 체크 */
	if (ofs_check_path_len(path) != 0)
		return -ENAMETOOLONG;
	
	/* node attribute 반환 */
	memset(stbuf, 0, sizeof(struct stat));					//stat 구조체 초기화
	if ((node = ofs_findnode(root, path)) != NULL) {
		stbuf -> st_ino = node -> of_stat -> of_id;
		stbuf -> st_mode = node -> of_stat -> of_mode;
		stbuf -> st_nlink = node -> of_stat -> of_nlink;
		stbuf -> st_uid = node -> of_stat -> of_uid;
		stbuf -> st_gid = node -> of_stat -> of_gid;
		stbuf -> st_size = node -> of_stat -> of_size;
		stbuf -> st_atime = node -> of_stat -> of_atime;
		stbuf -> st_mtime = node -> of_stat -> of_mtime;
		stbuf -> st_ctime = node -> of_stat -> of_ctime;
		return 0;
	} else {
		return -ENOENT;
	}
}

static int ofs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
	(void) offset;
	(void) fi;
	ONODE *loc, *cur;
	
	/* 에러 체크 */
	if (ofs_check_path_len(path) != 0)
		return -ENAMETOOLONG;
	if((loc = ofs_findnode(root, path)) == NULL)
		return -ENOENT;
	if(!S_ISDIR(loc-> of_stat -> of_mode)) 
		return -ENOTDIR;
	
	/* 디렉토리 이름 정보 반환 */
	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);
	for(cur = loc->subhead;cur != NULL;cur = cur->nextnode) {		//디렉토리의 서브 엔트리 반복
		filler(buf, cur->name, NULL, 0);						//디렉토리 채우기
	}

	return 0;
}

static int ofs_access(const char *path, int how) 
{
	ONODE *node = NULL;
	
	/* 에러 체크 */
	if (ofs_check_path_len(path) != 0)
		return -ENAMETOOLONG;
	if ((node = ofs_findnode(root, path)) == NULL) 
		return -ENOENT;
		
	/* 권한 체크 */	
	if(how == F_OK)								//파일 존재 여부 확인 
		return 0;
	return ofs_check_access(node -> of_stat -> of_mode, node -> of_stat -> of_uid, node -> of_stat -> of_gid, how);
}

static int ofs_utime(const char *path, struct utimbuf *times) 
{
	ONODE *node = NULL;
	OSTAT *stat = NULL;
	ONODE *parent;
	
	/* 에러 체크 */
	if (ofs_check_path_len(path) != 0) 
		return -ENAMETOOLONG;
	if ((node = ofs_findnode(root, path)) == NULL) 
		return -ENOENT;
	stat = node -> of_stat;
	if ((ofs_check_access(stat -> of_mode, stat -> of_uid, stat -> of_gid, W_OK)) != 0) 
		return -EACCES;
	parent = ofs_findparent(root, path);			//변경할 노드의 상위 정보 구하기
	if(*(parent->name) == '_')		// 타입 디렉토리에서 타입 노드 변경 불가
		return -EACCES;
	
	/* 시간 변경 */
	if(times == NULL) {									//NULL일경우 현재시간 저장
		node-> of_stat -> of_atime = time(NULL);
		node-> of_stat -> of_mtime = time(NULL);		
	} else {											//전달 받은 시간 저장
		if (node-> of_stat -> of_uid != fuse_get_context()->uid && fuse_get_context()->uid != 0) 	//소유자 이거나 Previliged인지 확인
			return -EPERM;
		node-> of_stat -> of_atime = times -> actime;		
		node-> of_stat -> of_mtime = times -> modtime;
	}
	return 0;
}

int ofs_unlink_node(const char *path) {
	ONODE *node = NULL, *parent = NULL;
	OSTAT *stat = NULL;
	
	/* 에러 체크 */
	if (ofs_check_path_len(path) != 0)
		return -ENAMETOOLONG;
	parent = ofs_findparent(root, path);			//삭제할 노드의 상위 정보 구하기
	stat = parent -> of_stat;
	if ((ofs_check_access(stat -> of_mode, stat -> of_uid, stat -> of_gid, W_OK | X_OK)) != 0) 
		return -EACCES;
	if((node = ofs_findnode(root, path)) == NULL) 
		return -ENOENT;
	if(S_ISDIR(node-> of_stat ->of_mode)) 
		return -EPERM;
	
	/* 파일 삭제 */
	if(node -> of_stat -> of_nlink == 1) {			//하드 링크 수가 1일 경우 - 실제 데이터와 노드정보를 삭제
		if(node -> of_data != NULL) free(node -> of_data);	
		if(node -> of_stat != NULL) free(node -> of_stat);
	} else {								//하드 링크수가 1이상일 경우 하드 링크수를 1 감소
		node -> of_stat -> of_nlink -= 1;		
	}
	free(ofs_deletenode(node));				//노드 제거
	return 0;
}

static int ofs_unlink(const char *path) {
	ONODE *parent = NULL, *typedir = NULL;
	char *file_name, *typedir_name;
	char *parent_path, *typedir_path, *link_path;
	char *extension;
	int ret;

	/* 에러 체크 */
	if (ofs_check_path_len(path) != 0)
		return -ENAMETOOLONG;
	parent = ofs_findparent(root, path);			//삭제할 노드의 상위 정보 구하기
	if(*(parent->name) == '_')		// 타입 디렉토리에서 타입 노드 삭제 불가
		return -EACCES;
	if(ofs_findnode(root, path) == NULL) 
		return -ENOENT;

	file_name = ofs_parsingname(path);
	extension = ofs_extension(file_name);
	if(extension != NULL) {
		// 확장자가 있는 경우, 타입 노드를 삭제한다.
		parent_path = ofs_parsingparent(path);
		typedir_name = ofs_typedirname(file_name);
		typedir_path = (char *)malloc(sizeof(char)*(strlen(parent_path)+strlen(typedir_name)+1));
		strcpy(typedir_path, parent_path);
		strcat(typedir_path, typedir_name);
		link_path = (char *)malloc(sizeof(char)*(strlen(typedir_path)+1+strlen(file_name)+1));
		strcpy(link_path, typedir_path);
		strcat(link_path, "/");
		strcat(link_path, file_name);
		ofs_unlink_node(link_path);

		// 타입 디렉토리가 비어버린 경우, 타입 디렉토리도 삭제한다.
		typedir = ofs_findnode(root, typedir_path);
		if(typedir->subhead == NULL) {
			ofs_removedir(typedir_path);
		}
		
		free(parent_path);
		free(typedir_name);
		free(typedir_path);
		free(link_path);
	}

	ret = ofs_unlink_node(path);
	return ret;
}

int ofs_removedir(const char *path) {
	ONODE *node = NULL, *parent = NULL;
	OSTAT *stat = NULL;
	
	/* 에러 체크 */
	if(strcmp(path, "/") == 0)					//루트(마운트 포인트) 삭제 불가 
		return -EBUSY;
	if (ofs_check_path_len(path) != 0) 
		return -ENAMETOOLONG;
	parent = ofs_findparent(root, path);
	stat = parent -> of_stat;
	if ((ofs_check_access(stat -> of_mode, stat -> of_uid, stat -> of_gid, W_OK | X_OK)) != 0) 
		return -EACCES;
	if((node = ofs_findnode(root, path)) == NULL) 
		return -ENOENT;
	if(node-> subhead != NULL) 			//부모 디렉토리가 비어있지 않은 경우
		return -ENOTEMPTY;
	if(!S_ISDIR(node-> of_stat ->of_mode)) 	//디렉토리가 아닌 경우
		return -EPERM;
	
	/* 디렉토리 삭제 */
	if(node -> of_stat != NULL) 
		free(node -> of_stat);				//디렉토리 노드 정보 삭제
	parent -> of_stat -> of_nlink -= 1;		//부모 디렉토리의 링크 수 감소
	free(ofs_deletenode(node));			//노드 제거	

	return 0;
}

static int ofs_rmdir(const char *path) {
	int ret;
	char *dir_name;

	dir_name = ofs_parsingname(path);
	if(*dir_name == '_')					// 타입 디렉토리는 삭제할 수 없다
		return -EACCES;

	ret = ofs_removedir(path);
	return ret;
}

// 디렉토리를 만든다.
int ofs_makedir(const char *path, mode_t mode) {
	ONODE *newdir = NULL, *target = NULL;
	OSTAT *stat = NULL;
	
	/* 에러 체크 */
	if (ofs_check_path_len(path) != 0) 
		return -ENAMETOOLONG;
	if (ofs_findnode(root, path) != NULL) 			//이미 존재하는 이름일 경우
		return -EEXIST;
	if((target = ofs_findparent(root, path)) == NULL) 
		return -ENOENT;	
	stat = target -> of_stat;
	if ((ofs_check_access(stat -> of_mode, stat -> of_uid, stat -> of_gid, W_OK | X_OK)) != 0) 
		return -EACCES;
	if(!S_ISDIR(target-> of_stat ->of_mode)) 			//Path 의 적합성 판단
		return -ENOTDIR;
	
	/* 디렉토리 생성 */
	target -> of_stat -> of_nlink++;					//부모 디렉토리의 링크 수 증가
	newdir = ofs_neONODE(ofs_parsingname(path), S_IFDIR | mode , fuse_get_context()->uid, fuse_get_context()->gid);
	ofs_insertnode(target, newdir);
	
	return 0;
}

// 일반 디렉토리를 만든다.
static int ofs_mkdir(const char *path, mode_t mode) {
	int ret;
	char *dir_name;
	ONODE *parent;

	/* 에러 체크 */
	if (ofs_check_path_len(path) != 0)
		return -ENAMETOOLONG;
	parent = ofs_findparent(root, path);			//삽입할 노드의 상위 정보 구하기
	if(*(parent->name) == '_')		// 타입 디렉토리에서는 생성 불가
		return -EACCES;
	dir_name = ofs_parsingname(path);
	if(*dir_name == '_')
		return -EINVAL;				// 디렉토리 이름은 _로 시작할 수 없다.

	ret = ofs_makedir(path, mode);
	return ret;
}

static int ofs_open(const char *path, struct fuse_file_info *fi)
{
	int how;
	ONODE *node = NULL;
	
	/* 에러 체크 */
	if (ofs_check_path_len(path) != 0) 
		return -ENAMETOOLONG;
	if ((node = ofs_findnode(root, path)) == NULL) 
		return -ENOENT;	
	if(S_ISDIR(node-> of_stat ->of_mode) && (fi-> flags & (O_WRONLY | O_RDWR))) //디렉터리 open시 처리
		return -EISDIR;	
	
	/* OPEN FLAG 정보 파싱 */
	if((fi -> flags & O_ACCMODE) == O_WRONLY) how = W_OK;
	else if((fi -> flags & O_ACCMODE) == O_RDONLY) how = R_OK;
	else how = W_OK | R_OK;
	
	/* 파일 권한 확인 */
	if ((ofs_check_access(node -> of_stat -> of_mode, node -> of_stat -> of_uid, node -> of_stat -> of_gid, how)) != 0) {
		return -EACCES;
	}
	
	return 0;
}

static int ofs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
	(void) fi;	
	int len;
	ONODE* node;
	
	/* 에러 체크 */
	if ((node = ofs_findnode(root, path)) == NULL) 
		return -ENOENT;
	
	/* 파일 읽기 */
	len = node -> of_stat -> of_size;					//데이터 길이 확인
	if (offset < len) {								//Offset이 데이터 길이를 넘어간 경우 에러
		if (offset + size > len)						//적합한 크기 구하기
			size = len - offset;
		memcpy(buf, node -> of_data + offset, size);	//데이터 읽기 버퍼에 저장
	} else {
		size = 0;
	}
	
	return size;
}


static int ofs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) 
{
	(void)fi;
	ONODE* node;
	ONODE *parent;
	
	if ((node = ofs_findnode(root, path)) == NULL) 
		return -ENOENT;
	parent = ofs_findparent(root, path);			//변경할 노드의 상위 정보 구하기
	if(*(parent->name) == '_')		// 타입 디렉토리에서 타입 노드 변경 불가
		return -EACCES;
			
	ofs_setdata(node, buf, size, offset);
	
	return size;
}

int ofs_rename_node(const char *oldname, const char *newname) 
{
	int ret;
	ONODE *oldp, *newp, *old, *new; 
	OSTAT *olds, *news;
	
	/* 에러 체크 */
	if (ofs_check_path_len(oldname) != 0 || ofs_check_path_len(newname) != 0)
		return -ENAMETOOLONG;
	if ((oldp = ofs_findparent(root, oldname)) == NULL 
		|| (newp = ofs_findparent(root, newname)) == NULL) 			//각각의 부모 디렉토리가 존재하는지 확인
			return -ENOENT;
	olds = oldp->of_stat;
	news = newp->of_stat;
	if ((ofs_check_access(olds->of_mode, olds->of_uid, olds->of_gid, W_OK | X_OK)) != 0
		|| (ofs_check_access(news->of_mode, news->of_uid, news->of_gid, W_OK | X_OK)) != 0)
			return -EACCES;
	if((old = ofs_findnode(root, oldname)) == NULL) 
		return -ENOENT;	
	if ((new = ofs_findnode(root, newname)) != NULL)
		if((ret = ofs_unlink(newname)) != 0) 
			return ret;
	
	/* 파일 이름 변경 */
	strcpy(old->name, ofs_parsingname(newname));
	if(oldp != newp) {
		oldp -> of_stat -> of_nlink--;					//oldname의 부모 디렉토리의 링크 수 감소	
		newp -> of_stat -> of_nlink++;					//newname의 부모 디렉토리의 링크 수 증가
		ofs_insertnode(newp, ofs_deletenode(old));
	}
	return 0;
}

static int ofs_rename(const char *oldname, const char *newname) 
{
	char *old_parent_path, *new_parent_path;
	ONODE *neONODE;
	int ret;

	// 변경 전후의 디렉토리 이름을 알아낸다.
	old_parent_path = ofs_parsingparent(oldname);
	new_parent_path = ofs_parsingparent(newname);

	/* 에러 체크 */
	// 변경 후의 디렉토리가 타입 디렉토리일 수 없다.
	fprintf(stderr, "** %s : %s\n", new_parent_path, ofs_parsingname(new_parent_path));
	fprintf(stderr, "** %s : %s\n", newname, ofs_parsingname(new_parent_path));
	if(*(ofs_parsingname(new_parent_path)) == '_')
		return -EACCES;
	if(*(ofs_parsingname(newname)) == '_')
		return -EACCES;

	// 노드의 이름을 바꾼다.
	ret = ofs_rename_node(oldname, newname);
	
	fprintf(stderr, "** %s to %s\n", oldname, newname);

	if(ret == 0) {
		// 변경된 노드가 디렉토리가 아닌 경우, 타입 링크를 변경해야 한다.
		neONODE = ofs_findnode(root, newname);
		if(S_ISDIR(neONODE->of_stat->of_mode) == 0) {
			char *old_extension, *new_extension;
			char *old_file_name, *new_file_name;
			char *old_typedir_path, *new_typedir_path;
			char *old_typedir_name, *new_typedir_name;
			char *old_link_path, *new_link_path;
			char *new_link_target;
			ONODE *typedir;

			// 변경 전후의 확장자명을 알아낸다.
			old_file_name = ofs_parsingname(oldname);
			new_file_name = ofs_parsingname(newname);
			old_extension = ofs_extension(old_file_name);
			new_extension = ofs_extension(new_file_name);

			// 변경 전후의 타입 링크 주소를 알아낸다.
			old_typedir_name = ofs_typedirname(old_file_name);

			old_typedir_path = (char *)malloc(sizeof(char)*(strlen(old_parent_path)+strlen(old_typedir_name)+1));
			strcpy(old_typedir_path, old_parent_path);
			strcat(old_typedir_path, old_typedir_name);
			old_link_path = (char *)malloc(sizeof(char)*(strlen(old_typedir_path)+1+strlen(old_file_name)+1));
			strcat(old_link_path, old_typedir_path);
			strcat(old_link_path, "/");
			strcat(old_link_path, old_file_name);
			
			new_typedir_name = ofs_typedirname(new_file_name);

			new_typedir_path = (char *)malloc(sizeof(char)*(strlen(new_parent_path)+strlen(new_typedir_name)+1));
			strcpy(new_typedir_path, new_parent_path);
			strcat(new_typedir_path, new_typedir_name);
			new_link_path = (char *)malloc(sizeof(char)*(strlen(new_typedir_path)+1+strlen(new_file_name)+1));
			strcat(new_link_path, new_typedir_path);
			strcat(new_link_path, "/");
			strcat(new_link_path, new_file_name);

			if(old_extension != NULL || new_extension != NULL) {
				// 타입 디렉토리 노드를 알아낸다.
				typedir = ofs_findnode(root, new_typedir_path);

				// 확장자가 있는 파일로 옮기며 타입 디렉토리가 없는 경우, 새로 만든다.
				if(new_extension != NULL && typedir == NULL) {
					ofs_newtypedir(new_typedir_path);
				}

				// 타입 노드를 옮긴다.
				new_link_target = (char *)malloc(sizeof(char)*(3+strlen(new_file_name)+1));
				strcpy(new_link_target, "../");
				strcat(new_link_target, new_file_name);
				// 확장자 있는 파일로부터 옮기는 경우
				if(old_extension != NULL) {
					// 타입 노드를 제거한다.
					ofs_unlink_node(old_link_path);
				}
				// 확장자 있는 파일로 옮기는 경우
				if(new_extension != NULL) {
					// 타입 노드를 생성한다.
					ofs_symlink(new_link_target, new_link_path);
				}

				// 확장자 있는 파일로부터 옮기는 경우, 타입 디렉토리가 비게 되면 지운다.
				if(old_extension != NULL) {
					typedir = ofs_findnode(root, old_typedir_path);
					if(typedir->subhead == NULL) {
						ofs_removedir(old_typedir_path);
					}
				}
			}

			free(old_typedir_path);
			free(new_typedir_path);
			free(old_typedir_name);
			free(new_typedir_name);
			free(old_link_path);
			free(new_link_path);
		}
	}
	free(old_parent_path);
	free(new_parent_path);
	return ret;
}

static int ofs_opendir(const char *path, struct fuse_file_info *fi) 
{
	int how;
	ONODE *node = NULL;

	/* 에러 체크 */
	if (ofs_check_path_len(path) != 0) 
		return -ENAMETOOLONG;
	if ((node = ofs_findnode(root, path)) == NULL) 
		return -ENOENT;
	
	/* OPEN FLAG 파싱*/
	if((fi -> flags & O_ACCMODE) == O_WRONLY) how = W_OK;
	else if((fi -> flags & O_ACCMODE) == O_RDONLY) how = R_OK;
	else how = W_OK | R_OK;
	
	/* 디렉토리 권한 확인*/
	if ((ofs_check_access(node -> of_stat -> of_mode, node -> of_stat -> of_uid, node -> of_stat -> of_gid, how)) != 0) {
		return -EACCES;
	}
	
	return 0;
}

static struct fuse_operations ofs_oper = {
	.access = ofs_access,
	.getattr = ofs_getattr,
	.readdir	= ofs_readdir,
	.mknod = ofs_mknod,
	.open = ofs_open,
	.read	= ofs_read,
	.write = ofs_write,
	.unlink = ofs_unlink,
	.readlink = ofs_readlink,
	.mkdir = ofs_mkdir,
	.rmdir = ofs_rmdir,
	.utime = ofs_utime,
	.symlink = ofs_symlink,
	.link = ofs_link,
	.chmod = ofs_chmod,
	.chown = ofs_chown,
	.truncate = ofs_truncate,
	.rename = ofs_rename,
	.opendir = ofs_opendir,
};

int main(int argc, char *argv[]) 
{
	int ret;
	root = ofs_neONODE("/", S_IFDIR | 0755, getuid(), getgid());
	ret = fuse_main(argc, argv, &ofs_oper, NULL);
	return ret;
}
