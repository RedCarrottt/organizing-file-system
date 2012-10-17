#ifndef __NODE_H
#define __NODE_H
#include <sys/types.h>
#include <limits.h>

typedef char byte_t;

typedef struct _OSTAT {
	ino_t		of_id;
	mode_t	of_mode;
	nlink_t	of_nlink;
	uid_t 	of_uid;
	gid_t 	of_gid;
	off_t		of_size;
	dev_t 	of_rdev;
	time_t	of_atime;
	time_t	of_mtime;
	time_t	of_ctime;
} OSTAT;

typedef struct _ONODE {
	char			name[NAME_MAX];
	OSTAT			*of_stat; 
	byte_t			*of_data;
	struct _ONODE	*nextnode;
	struct _ONODE	*prevnode;
	struct _ONODE	*parentdir;
	struct _ONODE	*subhead;
	struct _ONODE	*subtail;
} ONODE;

/*######################################
 이름 : ofs_setdata	
 요약 : 파일에 데이터 저장 함수
 매개변수 : ONODE* [ROOT], const char*[PATH], size_t [SIZE], off_t [OFFSET]
 반환값 : 없음
 #######################################*/
void 		ofs_setdata		(ONODE*, const char*, size_t, off_t);

/*######################################
 이름 : ofs_neONODE
 요약 : 새로운 노드를 생성
 매개변수 : const char* [PATH], mode_t [MODE], uid_t [OWNER], gid_t[GROUP]
 반환값 : 생성된 노드
 #######################################*/
ONODE* 	ofs_neONODE		(const char*, mode_t, uid_t, gid_t);

/*######################################
 이름 : ofs_insertnode
 요약 : 노드를 삽입해준다.
 매개변수 : ONODE* [TARGET], ONODE* [NODE]
 반환값 : 삽입된 노드
 #######################################*/
ONODE* 	ofs_insertnode	(ONODE*, ONODE*);

/*######################################
 이름 : ofs_deletenode
 요약 : 노드 제거 함수
 매개변수 : ONODE* [NODE]
 반환값 : 제거된 노드
 #######################################*/
ONODE*	ofs_deletenode	(ONODE*);

/*######################################
 이름 : ofs_setdata	
 요약 : 루트에서 패스에 해당하는 노드 검색
 매개변수 : ONODE* [ROOT], const char*[PATH]
 반환값 : 찾은 노드
 #######################################*/
ONODE* 	ofs_findnode		(ONODE*, const char *);

/*######################################
 이름 : ofs_setdata	
 요약 : 루트에서 패스의 부모 노드 검색
 매개변수 : ONODE* [ROOT], const char*[PATH]
 반환값 : 찾은 노드
 #######################################*/
ONODE* 	ofs_findparent		(ONODE*, const char *); 

#endif

