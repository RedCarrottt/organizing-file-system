#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include "node.h"

ino_t inumber=1;

void ofs_setdata(ONODE* target, const char* buffer, size_t size, off_t offset) 
{	
	/* 저장 공간 확보 */
	if(offset != 0) {											//파일의 뒷부분에 연결 하는 경우
		target->of_data = (byte_t*)realloc(target->of_data, offset + size);
	} else {
		if(target -> of_data != NULL) {							//저장 된 데이터가 있을 경우
			target->of_data = (byte_t*)realloc(target->of_data,size);
		} else {
			target->of_data = (byte_t*)malloc(size);				//처음 데이터를 저장
		}
	}
	
	/* 데이터 저장 */
	memcpy(target->of_data+offset, buffer, size);
	target->of_stat->of_size = size + offset;						//파일 사이즈 반영
}

ONODE* ofs_neONODE(const char* _name, mode_t _mode, uid_t _uid, gid_t _gid) 
{
	ONODE* ret = (ONODE*)malloc(sizeof(ONODE));					//노드 생성			
	OSTAT* stat = (OSTAT*)malloc(sizeof(OSTAT));					//파일 정보 구조체 생성

	/* 파일 정보 초기화*/
	stat -> of_id = inumber++;
	stat -> of_mode = _mode;
	stat -> of_nlink = (S_ISDIR(_mode))?2:1; 
	stat -> of_size = 0;
	stat -> of_uid = _uid;
	stat -> of_gid = _gid;
	stat -> of_rdev = 0;
	stat -> of_atime = stat -> of_mtime = stat -> of_ctime = time(NULL);
	
	/* 노드 초기화 */
	strcpy(ret -> name, _name);
	ret -> of_stat = stat;
	ret -> of_data = NULL;		//데이터 필드 초기화
	ret -> nextnode = NULL;
	ret -> prevnode = NULL;
	ret -> parentdir = NULL;
	ret -> subhead = NULL;
	ret -> subtail = NULL;

	return ret;
}  

ONODE* ofs_insertnode(ONODE* target, ONODE* node) 
{
	if(target -> subhead == NULL) {				//비어 있는 디렉터리 넣을 경우
		node -> prevnode = NULL;
		target -> subhead = node;
	} else {								//다른 하위 노드가 있을 경우
		target -> subtail -> nextnode = node;
		node -> prevnode = target -> subtail;
	}
	node -> nextnode = NULL;
	node -> parentdir = target;					//부모 디렉터리 지정
	target -> subtail = node;

	return node;
}

ONODE* ofs_deletenode(ONODE* node) {
	if(node->prevnode == NULL && node->nextnode ==  NULL) {			// 단일 서브 노드 였을 경우
		node->parentdir -> subhead = node->parentdir -> subtail =  NULL;
	} else if(node->prevnode == NULL) {							// 헤드 노드 였을 경우
		node->nextnode->prevnode = NULL;
		node->parentdir->subhead = node->nextnode;
	} else if(node->nextnode ==  NULL) {							// 테일 노드 였을 경우
		node->prevnode->nextnode = NULL;
		node->parentdir->subtail = node->prevnode;
	} else {												// 그외의 경우
		node->nextnode->prevnode = node -> prevnode;
		node->prevnode->nextnode = node -> nextnode;
	}
	return node;
}

ONODE* ofs_findnode(ONODE* root, const char *path) {
	ONODE *cur, *loc;
	char tpath[PATH_MAX];
	char *p;	
	
	strcpy(tpath, path);
	
	p = strtok(tpath+1, "/");
	
	loc = cur = root;	
	while(p != NULL) {
		loc = NULL;
		if(cur->subhead == NULL) return NULL;
		for(cur=cur->subhead; cur  != NULL ; cur=cur->nextnode) {	//하위 디렉터리에서 Sibling 노드들을 검색
			if(strcmp(cur->name, p) == 0) {					//해당하는 노드를 찾았을 경우
				loc = cur;
				break;
			}
		}
		p = strtok(NULL,"/");								//다음 이름 검색
	}
	return loc;
}

ONODE* ofs_findparent(ONODE* root, const char *path) {
	ONODE *cur, *loc;
	char tpath[PATH_MAX];
	char *p;	
	
	strcpy(tpath, path);
	
	p = strrchr(tpath, '/');									//마지막 이름은 무시(부모 디렉토리를 찾게됨)
	
	*(p+1) = '\0';
	
	p = strtok(tpath+1, "/");
	
	loc = cur = root;	
	while(p != NULL) {
		loc = NULL;
		if(cur->subhead == NULL) return NULL;
		for(cur=cur->subhead; cur  != NULL ; cur=cur->nextnode) {
			if(strcmp(cur->name, p) == 0) {
				loc = cur;
				break;
			}
		}
		p = strtok(NULL,"/");
	}
	return loc;
}



