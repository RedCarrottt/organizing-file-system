#ifndef __LIB_H
#define __LIB_H

#include <sys/types.h>

/*######################################
 이름 : ofs_check_path_len
 요약 : 경로와 파일 이름의 무결성 판단 함수
 매개변수 : const char* [Path]
 반환값 : 성공시 0, 실패시 음수
 #######################################*/
int 		ofs_check_path_len		(const char *);

/*######################################
 이름 : ofs_check_access
 요약 : 주어진 mode의 Permission 확인
 매개변수 : mode_t [PERM], uid_t [USER_ID], gid_t [GROUP_ID], int [HOW]
 반환값 : 성공시 0, 실패시 음수
 #######################################*/
int 		ofs_check_access		(mode_t, uid_t, gid_t, int);

/*######################################
 이름 : ofs_parsingname
 요약 : Path에서 마지막 경로의 이름만 추출
 매개변수 : const char* [Path]
 반환값 : 경로의 마지막 이름
 #######################################*/
char* 	ofs_parsingname		(const char *);

/*######################################
 이름 : ofs_parsingparent
 요약 : Path에서 부모 디렉토리 경로 추출
 매개변수 : const char* [Path]
 반환값 : 부모 디렉토리의 경로
 #######################################*/
char* 	ofs_parsingparent		(const char *);

/*######################################
 이름 : ofs_extension
 요약 : Path의 확장자 이름 추출
 매개변수 : const char* [Path]
 반환값 : 확장자 이름 
 #######################################*/
char* 	ofs_extension		(const char *);

/*######################################
 이름 : ofs_typedirname
 요약 : Path에 해당하는 타입 디렉토리 이름 추출
 매개변수 : const char* [Path]
 반환값 : 타입 디렉토리 이름
 #######################################*/
char* 	ofs_typedirname	(const char *);

#endif

