//
// Simple FIle System
// Student Name : 정근화
// Student Number : B354025
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* optional */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
/***********/

#include "sfs_types.h"
#include "sfs_func.h"
#include "sfs_disk.h"
#include "sfs.h"

void dump_directory();

/* BIT operation Macros */
/* a=target variable, b=bit number to act upon 0-n */
#define BIT_SET(a, b) ((a) |= (1 << (b)))
#define BIT_CLEAR(a, b) ((a) &= ~(1 << (b)))
#define BIT_FLIP(a, b) ((a) ^= (1 << (b)))
#define BIT_CHECK(a, b) ((a) & (1 << (b)))

static struct sfs_super spb;				// superblock
static struct sfs_dir sd_cwd = {SFS_NOINO}; // current working directory

void error_message(const char *message, const char *path, int error_code)
{
	switch (error_code)
	{
	case -1:
		printf("%s: %s: No such file or directory\n", message, path);
		return;
	case -2:
		printf("%s: %s: Not a directory\n", message, path);
		return;
	case -3:
		printf("%s: %s: Directory full\n", message, path);
		return;
	case -4:
		printf("%s: %s: No block available\n", message, path);
		return;
	case -5:
		printf("%s: %s: Not a directory\n", message, path);
		return;
	case -6:
		printf("%s: %s: Already exists\n", message, path);
		return;
	case -7:
		printf("%s: %s: Directory not empty\n", message, path);
		return;
	case -8:
		printf("%s: %s: Invalid argument\n", message, path);
		return;
	case -9:
		printf("%s: %s: Is a directory\n", message, path);
		return;
	case -10:
		printf("%s: %s: Is not a file\n", message, path);
		return;
	default:
		printf("unknown error code\n");
		return;
	}
}
void sfs_mount(const char *path)
{
	if (sd_cwd.sfd_ino != SFS_NOINO)
	{
		//umount
		disk_close();
		printf("%s, unmounted\n", spb.sp_volname);
		bzero(&spb, sizeof(struct sfs_super));
		sd_cwd.sfd_ino = SFS_NOINO;
	}

	printf("Disk image: %s\n", path);

	disk_open(path);
	disk_read(&spb, SFS_SB_LOCATION);

	printf("Superblock magic: %x\n", spb.sp_magic);

	assert(spb.sp_magic == SFS_MAGIC);

	printf("Number of blocks: %d\n", spb.sp_nblocks);
	printf("Volume name: %s\n", spb.sp_volname);
	printf("%s, mounted\n", spb.sp_volname);

	sd_cwd.sfd_ino = 1; //init at root
	sd_cwd.sfd_name[0] = '/';
	sd_cwd.sfd_name[1] = '\0';
}

void sfs_umount()
{

	if (sd_cwd.sfd_ino != SFS_NOINO)
	{
		//umount
		disk_close();
		printf("%s, unmounted\n", spb.sp_volname);
		bzero(&spb, sizeof(struct sfs_super));
		sd_cwd.sfd_ino = SFS_NOINO;
	}
}

//디스크의 0인 비트맵을 1로 바꿔서 디스크이미지에 저장한다.
void newBitmap(int targetIno)
{

	//number of Bitmap Blocks
	int bitmapBlocks = SFS_BITBLOCKS(spb.sp_nblocks);
	int i, j, blocksNo, nodeCount = 0, find = 0;
	for (blocksNo = 2; blocksNo < 2 + bitmapBlocks; blocksNo++)
	{
		u_int8_t bits[SFS_BLOCKBITS];
		disk_read(bits, blocksNo);
		for (i = 0; i < SFS_BLOCKSIZE; i++)
		{
			u_int8_t tp = bits[i];
			for (j = 0; j < CHAR_BIT; j++)
			{
				if (tp % 2 == 0 && nodeCount == targetIno)
				{
					//printf("(%d)", nodeCount);
					find = 1;
					int mod = targetIno % CHAR_BIT;
					int add = 1 << mod;
					bits[i] += add;
					disk_write(bits, blocksNo);
					break;
				}
				tp /= 2;
				nodeCount++;
			}
			if (find == 1)
				break;
		}
		if (find == 1)
			break;
	}
}

//디스크의 1인 비트맵을 0으로 바꿔서 디스크이미지에 저장한다.
void deleteBitmap(int targetIno)
{
	//number of Bitmap Blocks
	int bitmapBlocks = SFS_BITBLOCKS(spb.sp_nblocks);
	int i, j, blocksNo, nodeCount = 0, find = 0;
	for (blocksNo = 2; blocksNo < 2 + bitmapBlocks; blocksNo++)
	{
		u_int8_t bits[SFS_BLOCKBITS];
		disk_read(bits, blocksNo);
		for (i = 0; i < SFS_BLOCKSIZE; i++)
		{
			u_int8_t tp = bits[i];
			for (j = 0; j < CHAR_BIT; j++)
			{
				if (tp % 2 == 1 && nodeCount == targetIno)
				{
					//printf("(%d)", nodeCount);
					find = 1;
					int mod = targetIno % CHAR_BIT;
					int add = 1 << mod;
					bits[i] -= add;
					disk_write(bits, blocksNo);
					break;
				}
				tp /= 2;
				nodeCount++;
			}
			if (find == 1)
				break;
		}
		if (find == 1)
			break;
	}
}

//디스크의 비트맵 블럭들을 모두 읽어 가장 앞쪽인덱스의 빈 노드번호를 찾는다
//만약 빈 노드번호가 하나도 없다면 -1을 반환할 것이다.
int getEmptyNode()
{

	//number of Bitmap Blocks
	int bitmapBlocks = SFS_BITBLOCKS(spb.sp_nblocks);
	int i, j, blocksNo, nodeCount = 0, find = 0;
	for (blocksNo = 2; blocksNo < 2 + bitmapBlocks; blocksNo++)
	{
		u_int8_t bits[SFS_BLOCKBITS];
		disk_read(bits, blocksNo);
		for (i = 0; i < SFS_BLOCKSIZE; i++)
		{
			u_int8_t tp = bits[i];
			for (j = 0; j < CHAR_BIT; j++)
			{
				if (tp % 2 == 0)
				{
					//printf("(%d)", nodeCount);
					find = 1;
					break;
				}
				tp /= 2;
				nodeCount++;
			}
			if (find == 1)
				break;
		}
		if (find == 1)
			break;
	}
	return (find == 1) ? nodeCount : -1;
}

void printBitmapInteger()
{
	//number of Bitmap Blocks
	int bitmapBlocks = SFS_BITBLOCKS(spb.sp_nblocks);
	int i, j, blocksNo, nodeCount = 0, find = 0;
	for (blocksNo = 2; blocksNo < 2 + bitmapBlocks; blocksNo++)
	{
		u_int8_t bits[SFS_BLOCKBITS];
		disk_read(bits, blocksNo);
		printf("blocksNo : %d\n", blocksNo);
		for (i = 0; i < SFS_BLOCKSIZE; i++)
		{
			u_int8_t tp = bits[i];
			printf("%d", bits[i]);
			// for(j = 0; j < CHAR_BIT; j++){
			// 	printf("%d", tp%2);
			// }
			printf("\n");
		}
	}
}

// 주어진 path를 현재 노드와 일치하는 inode를 찾음.
// 만약 일치하는 것이 없다면 -1 반환
int searchNode(const char *path)
{
	struct sfs_inode curNode;
	disk_read(&curNode, sd_cwd.sfd_ino);
	int i, j;
	int findPath = 0, iNodeNo;
	for (i = 0; i < SFS_NDIRECT; i++)
	{
		struct sfs_dir dirs[SFS_DENTRYPERBLOCK];
		if (curNode.sfi_direct[i] == 0 || findPath == 1)
			break;

		disk_read(dirs, curNode.sfi_direct[i]);
		for (j = 0; j < SFS_DENTRYPERBLOCK; j++)
		{
			if (strcmp(dirs[j].sfd_name, path) == 0)
			{
				//find Path
				findPath = 1;
				iNodeNo = dirs[j].sfd_ino;
				break;
			}
		}
	}
	if (findPath == 0)
	{
		//can't find
		return -1;
	}
	else
	{
		return iNodeNo;
	}
}

void sfs_touch(const char *path)
{
	//skeleton implementation

	struct sfs_inode si;
	disk_read(&si, sd_cwd.sfd_ino);

	//for consistency
	assert(si.sfi_type == SFS_TYPE_DIR);

	//we assume that cwd is the root directory and root directory is empty which has . and .. only
	//unused DISK2.img satisfy these assumption
	//for new directory entry(for new file), we use cwd.sfi_direct[0] and offset 2
	//becasue cwd.sfi_directory[0] is already allocated, by .(offset 0) and ..(offset 1)
	//for new inode, we use block 6
	// block 0: superblock,	block 1:root, 	block 2:bitmap
	// block 3:bitmap,  	block 4:bitmap 	block 5:root.sfi_direct[0] 	block 6:unused
	//
	//if used DISK2.img is used, result is not defined

	//buffer for disk read

	//===================================================

	// //allocate new block
	int newbie_ino;

	//이름중복검사 해야함!!!!!!!!!!!!!!!!!!!!
	int joongbok = searchNode(path);
	if (joongbok != -1)
	{
		//중복된 이름이 존재한다.
		error_message("touch", path, -6);
		return;
	}

	////////////////////////////////////////

	// //block access
	// disk_read(sd, si.sfi_direct[0]);
	int i, j, isfind = 0;
	struct sfs_dir dirs[SFS_DENTRYPERBLOCK];
	//폴더디렉션에서 빈 디렉션 찾기.
	for (i = 0; i < SFS_NDIRECT; i++)
	{
		if (si.sfi_direct[i] == 0)
		{
			//여기는 아직 할당 안됨. 만약 여기 디렉션까지 왔는데 아직 못찾았다면
			//새로운 디렉션을 먼저 할당 해주어야 함. size변경은 안해줘도됨
			int tpNewInode = getEmptyNode();
			if (tpNewInode == -1)
			{
				error_message("touch", path, -4);
				return;
			}
			//현재 inode 의 디렉터리 추가된것 갱신
			si.sfi_direct[i] = tpNewInode;
			disk_write(&si, sd_cwd.sfd_ino);

			//새로운 노드에 디렉터리 inode 추가.
			struct sfs_dir newDirs[SFS_DENTRYPERBLOCK];
			for (j = 0; j < SFS_DENTRYPERBLOCK; j++)
			{
				newDirs[j].sfd_ino = SFS_NOINO;
				strncpy(newDirs[j].sfd_name, "", SFS_NAMELEN);
			}
			disk_write(newDirs, si.sfi_direct[i]);

			//bitmap 정보 갱신
			newBitmap(tpNewInode);
		}
		//direct[i] 노드 진입

		disk_read(dirs, si.sfi_direct[i]);
		//디렉터리 정보 받음.
		for (j = 0; j < SFS_DENTRYPERBLOCK; j++)
		{
			if (dirs[j].sfd_ino == SFS_NOINO)
			{
				//빈 블럭 찾음. 여기다가 할당해주면됨.
				// 디스크 i 번 블럭에 dirs[j] 번에다가 추가해줌.
				isfind = 1;
				newbie_ino = getEmptyNode();
				//가득차서 다른 빈 inode가 존재하지 않음.
				//printf("newInode : %d\n", newbie_ino);
				if (newbie_ino == -1)
				{
					error_message("touch", path, -4);
					return;
				}
				break;
			}
		}

		if (isfind == 1)
			break;
	}

	if (isfind == 1)
	{
		// sd[2].sfd_ino = newbie_ino;
		// strncpy(sd[2].sfd_name, path, SFS_NAMELEN);
		// disk_write(sd, si.sfi_direct[0]);

		//디렉션 노드의 dirs 배열정보 추가해서 update
		dirs[j].sfd_ino = newbie_ino;
		strncpy(dirs[j].sfd_name, path, SFS_NAMELEN);
		disk_write(dirs, si.sfi_direct[i]);

		// si.sfi_size += sizeof(struct sfs_dir);
		// disk_write(&si, sd_cwd.sfd_ino);

		//현재노드 하위 dirs 가 하나 추가되었으므로 사이즈정보 update
		si.sfi_size += sizeof(struct sfs_dir);
		disk_write(&si, sd_cwd.sfd_ino);

		//실제 새로 배당받은 디스크의 새로운 노드에 inode타입 입력.
		struct sfs_inode newbie;
		bzero(&newbie, SFS_BLOCKSIZE);
		newbie.sfi_size = 0;
		newbie.sfi_type = SFS_TYPE_FILE;
		disk_write(&newbie, newbie_ino);

		//비트맵정보 갱신해야함.
		newBitmap(newbie_ino);
	}
	else
	{
		// 디렉터리가 완전 꽉찬상태. 8*15 모두 차있음.
		error_message("touch", path, -3);
	}

	// struct sfs_inode newbie;

	// bzero(&newbie, SFS_BLOCKSIZE); // initalize sfi_direct[] and sfi_indirect
	// newbie.sfi_size = 0;
	// newbie.sfi_type = SFS_TYPE_FILE;

	// disk_write(&newbie, newbie_ino);

	// //+++ bitmap 정보도 수정해주어야함.
}

void sfs_cd(const char *path)
{
	//printf("Not Implemented\n");
	if (path == NULL)
	{
		//change root directory
		sd_cwd.sfd_ino = SFS_ROOT_LOCATION;
		sd_cwd.sfd_name[0] = '/';
		sd_cwd.sfd_name[1] = '\0';
	}
	else
	{
		int iNodeNo = searchNode(path);
		if (iNodeNo == -1)
		{
			//error can't find
			error_message("cd", path, -1);
		}
		else
		{
			//is file?
			struct sfs_inode tpInode;
			disk_read(&tpInode, iNodeNo);
			if (tpInode.sfi_type == SFS_TYPE_FILE)
			{
				error_message("cd", path, -2);
				return;
			}
			//this is dirs
			sd_cwd.sfd_ino = iNodeNo;
			strcpy(sd_cwd.sfd_name, path);
		}
	}
}

void go_ls(const int iNodeNo, const char *path)
{
	struct sfs_inode curNode;
	disk_read(&curNode, iNodeNo);
	if (curNode.sfi_type == SFS_TYPE_FILE)
	{
		printf("%s\n", path);
		return;
	}
	int i, j, eof = 0;
	for (i = 0; i < SFS_NDIRECT; i++)
	{
		struct sfs_dir dirs[SFS_DENTRYPERBLOCK];
		if (curNode.sfi_direct[i] == 0)
		{
			// if (eof)
			printf("\n");
			break;
		}
		disk_read(dirs, curNode.sfi_direct[i]);
		for (j = 0; j < SFS_DENTRYPERBLOCK; j++)
		{
			if (dirs[j].sfd_ino == 0)
			{
				continue;
				// printf("\n");
				// break;
				// eof = 1;
			}
			struct sfs_inode iNode;
			printf("%s", dirs[j].sfd_name);
			disk_read(&iNode, dirs[j].sfd_ino);
			if (iNode.sfi_type == SFS_TYPE_DIR)
				printf("/");
			printf("\t");
		}
	}
}

void sfs_ls(const char *path)
{
	//printf("Not Implemented\n");

	if (path == NULL)
	{
		//root Path
		go_ls(sd_cwd.sfd_ino, NULL);
	}
	else
	{
		//find Path
		int iNodeNo = searchNode(path);
		if (iNodeNo == -1)
		{
			//can't find coincidence path
			error_message("ls", path, -1);
		}
		else
		{
			//ls findPath
			go_ls(iNodeNo, path);
		}
	}
}

void sfs_mkdir(const char *path)
{
	//printf("Not Implemented\n");

	struct sfs_inode si;
	disk_read(&si, sd_cwd.sfd_ino);

	//for consistency
	assert(si.sfi_type == SFS_TYPE_DIR);

	int newbie_ino;

	//이름중복검사 해야함!!!!!!!!!!!!!!!!!!!!
	int joongbok = searchNode(path);
	if (joongbok != -1)
	{
		//중복된 이름이 존재한다.
		error_message("mkdir", path, -6);
		return;
	}
	////////////////////////////////////////
	//block access
	// disk_read(sd, si.sfi_direct[0]);
	int i, j, isfind = 0;
	struct sfs_dir dirs[SFS_DENTRYPERBLOCK];
	//폴더디렉션에서 빈 디렉션 찾기.
	for (i = 0; i < SFS_NDIRECT; i++)
	{
		if (si.sfi_direct[i] == 0)
		{
			//여기는 아직 할당 안됨. 만약 여기 디렉션까지 왔는데 아직 못찾았다면
			//새로운 디렉션을 먼저 할당 해주어야 함. size변경은 안해줘도됨
			int tpNewInode = getEmptyNode();
			if (tpNewInode == -1)
			{
				error_message("mkdir", path, -4);
				return;
			}
			//현재 inode 의 디렉터리 추가된것 갱신
			si.sfi_direct[i] = tpNewInode;
			disk_write(&si, sd_cwd.sfd_ino);

			//새로운 노드에 디렉터리 inode 추가.
			struct sfs_dir newDirs[SFS_DENTRYPERBLOCK];
			for (j = 0; j < SFS_DENTRYPERBLOCK; j++)
			{
				newDirs[j].sfd_ino = SFS_NOINO;
				strncpy(newDirs[j].sfd_name, "", SFS_NAMELEN);
			}
			disk_write(newDirs, si.sfi_direct[i]);

			//bitmap 정보 갱신
			newBitmap(tpNewInode);
		}
		//direct[i] 노드 진입

		disk_read(dirs, si.sfi_direct[i]);
		//디렉터리 정보 받음.
		for (j = 0; j < SFS_DENTRYPERBLOCK; j++)
		{
			if (dirs[j].sfd_ino == SFS_NOINO)
			{
				//빈 블럭 찾음. 여기다가 할당해주면됨.
				// 디스크 i 번 블럭에 dirs[j] 번에다가 추가해줌.
				isfind = 1;
				newbie_ino = getEmptyNode();
				//가득차서 다른 빈 inode가 존재하지 않음.
				//printf("newInode : %d\n", newbie_ino);
				if (newbie_ino == -1)
				{
					error_message("mkdir", path, -4);
					return;
				}
				break;
			}
		}

		if (isfind == 1)
			break;
	}

	if (isfind == 1)
	{
		// sd[2].sfd_ino = newbie_ino;
		// strncpy(sd[2].sfd_name, path, SFS_NAMELEN);
		// disk_write(sd, si.sfi_direct[0]);

		//디렉션 노드의 dirs 배열정보 추가해서 update
		dirs[j].sfd_ino = newbie_ino;
		strncpy(dirs[j].sfd_name, path, SFS_NAMELEN);
		disk_write(dirs, si.sfi_direct[i]);

		// si.sfi_size += sizeof(struct sfs_dir);
		// disk_write(&si, sd_cwd.sfd_ino);

		//현재노드 하위 dirs 가 하나 추가되었으므로 사이즈정보 update
		si.sfi_size += sizeof(struct sfs_dir);
		disk_write(&si, sd_cwd.sfd_ino);

		//먼저 새로운 디렉터리 inode에 새로운 디렉터리 노드를 추가해주고 . 와 .. 폴더를 연결시켜준다.
		//실제 새로 배당받은 디스크의 새로운 노드에 inode타입 입력.
		struct sfs_inode newbie;
		bzero(&newbie, SFS_BLOCKSIZE);
		newbie.sfi_size = 0;
		newbie.sfi_type = SFS_TYPE_DIR;
		disk_write(&newbie, newbie_ino);

		//비트맵정보 갱신해야함.
		newBitmap(newbie_ino);

		//여기는 아직 할당 안됨. 만약 여기 디렉션까지 왔는데 아직 못찾았다면
		//새로운 디렉션을 먼저 할당 해주어야 함. size변경은 안해줘도됨
		int tpNewInode = getEmptyNode();
		if (tpNewInode == -1)
		{
			error_message("mkdir", path, -4);
			return;
		}

		// newbieinode 의 디렉터리 추가된것 갱신
		newbie.sfi_direct[0] = tpNewInode;
		disk_write(&newbie, newbie_ino);

		//새로운 노드에 디렉터리 inode 추가.
		struct sfs_dir newDirs[SFS_DENTRYPERBLOCK];
		for (j = 0; j < SFS_DENTRYPERBLOCK; j++)
		{
			newDirs[j].sfd_ino = SFS_NOINO;
			strncpy(newDirs[j].sfd_name, "", SFS_NAMELEN);
		}
		newDirs[0].sfd_ino = newbie_ino;
		strncpy(newDirs[0].sfd_name, ".", SFS_NAMELEN);
		newbie.sfi_size += sizeof(struct sfs_dir);
		newDirs[1].sfd_ino = sd_cwd.sfd_ino;
		strncpy(newDirs[1].sfd_name, "..", SFS_NAMELEN);
		newbie.sfi_size += sizeof(struct sfs_dir);
		disk_write(newDirs, newbie.sfi_direct[0]);
		disk_write(&newbie, newbie_ino);

		//bitmap 정보 갱신
		newBitmap(tpNewInode);
	}
	else
	{
		// 디렉터리가 완전 꽉찬상태. 8*15 모두 차있음.
		error_message("mkdir", path, -3);
	}
}

void sfs_rmdir(const char *path)
{
	//printf("Not Implemented\n");
	int tarNode;

	if (strcmp(".", path) == 0)
	{
		error_message("rmdir", path, -8);
		return;
	}

	//현재 디스크 탐색.
	struct sfs_inode curNode;
	disk_read(&curNode, sd_cwd.sfd_ino);
	int i, j, k;
	int findPath = 0;
	for (i = 0; i < SFS_NDIRECT; i++)
	{
		struct sfs_dir dirs[SFS_DENTRYPERBLOCK];
		if (curNode.sfi_direct[i] == 0 || findPath == 1)
			break;

		disk_read(dirs, curNode.sfi_direct[i]);
		for (j = 0; j < SFS_DENTRYPERBLOCK; j++)
		{
			if (strcmp(dirs[j].sfd_name, path) == 0)
			{
				//find Path
				findPath = 1;
				tarNode = dirs[j].sfd_ino;

				//path가 존재한다면 디렉터리가 맞는지 확인
				struct sfs_inode delNode;
				disk_read(&delNode, tarNode);
				if (delNode.sfi_type != SFS_TYPE_DIR)
				{
					error_message("rmdir", path, -5);
					return;
				}
				//디렉터리가 맞다면 하위 엔트리가 존재하는지 확인(비어있어야함 128byte - ./, ../ 2개만 존재)
				if (delNode.sfi_size > sizeof(struct sfs_dir) * 2)
				{
					//엔트리 존재
					error_message("rmdir", path, -7);
					return;
				}
				//예외처리완료-------------------------------
				//삭제해야힘
				//cwd curNode 에서 해당 디렉터리의 엔트리 ino 를  SFS NOINO 으로 바꿈.
				dirs[j].sfd_ino = SFS_NOINO;
				disk_write(dirs, curNode.sfi_direct[i]);

				//해당 디렉터리가 가지고 있던 모든 디렉터리 inode 비트맵 해제
				for (k = 0; k < SFS_NDIRECT; k++)
				{
					if (delNode.sfi_direct[k] == 0)
						break;
					deleteBitmap(delNode.sfi_direct[k]);
				}

				//curNode의 사이즈를 줄임
				curNode.sfi_size -= sizeof(struct sfs_dir);
				disk_write(&curNode, sd_cwd.sfd_ino);

				//비트맵에서 해당 노드를 1->0으로 삭제 갱신.
				deleteBitmap(tarNode);

				break;
			}
		}
	}
	//해당 for문을 전부 돌았는데도 path를 찾지 못함.
	if (findPath == 0)
	{
		error_message("rmdir", path, -1);
		return;
	}
}

void sfs_mv(const char *src_name, const char *dst_name)
{
	//printf("Not Implemented\n");

	struct sfs_inode curNode;
	disk_read(&curNode, sd_cwd.sfd_ino);
	int i, j;
	int findPath = 0, iNodeNo;
	for (i = 0; i < SFS_NDIRECT; i++)
	{
		struct sfs_dir dirs[SFS_DENTRYPERBLOCK];
		if (curNode.sfi_direct[i] == 0 || findPath == 1)
			break;

		disk_read(dirs, curNode.sfi_direct[i]);
		for (j = 0; j < SFS_DENTRYPERBLOCK; j++)
		{
			if (strcmp(dirs[j].sfd_name, src_name) == 0)
			{
				//find Path
				findPath = 1;
				iNodeNo = dirs[j].sfd_ino;

				//바꾸고자 하는 폴더를 찾음.
				//중복검사해야함.
				int isJoongbok = searchNode(dst_name);
				if (isJoongbok != -1)
				{
					//일치하는 것이 존재함
					//바꾸려는 파일-디렉터리명이 이미 존재함.
					error_message("mv", dst_name, -6);
					return;
				}

				//폴더명을 바꾸어줌.
				strncpy(dirs[j].sfd_name, dst_name, SFS_NAMELEN);
				disk_write(dirs, curNode.sfi_direct[i]);

				break;
			}
		}
	}
	if (findPath == 0)
	{
		//can't find
		//바꾸고자 하는 폴더명이 없음.
		error_message("mv", src_name, -1);
		return;
	}
}

void sfs_rm(const char *path)
{
	//printf("Not Implemented\n");

	int tarNode;

	//현재 디스크 탐색.
	struct sfs_inode curNode;
	disk_read(&curNode, sd_cwd.sfd_ino);
	int i, j, k;
	int findPath = 0;
	for (i = 0; i < SFS_NDIRECT; i++)
	{
		struct sfs_dir dirs[SFS_DENTRYPERBLOCK];
		if (curNode.sfi_direct[i] == 0 || findPath == 1)
			break;

		disk_read(dirs, curNode.sfi_direct[i]);
		for (j = 0; j < SFS_DENTRYPERBLOCK; j++)
		{
			if (strcmp(dirs[j].sfd_name, path) == 0)
			{
				//find Path
				findPath = 1;
				tarNode = dirs[j].sfd_ino;

				//path가 존재한다면 파일 맞는지 확인
				struct sfs_inode delNode;
				disk_read(&delNode, tarNode);
				if (delNode.sfi_type != SFS_TYPE_FILE)
				{
					error_message("rm", path, -9);
					return;
				}

				//예외처리완료-------------------------------
				//삭제해야힘
				//cwd curNode 에서 해당 디렉터리의 엔트리 ino 를  SFS NOINO 으로 바꿈.
				dirs[j].sfd_ino = SFS_NOINO;
				disk_write(dirs, curNode.sfi_direct[i]);

				//해당 파일이 가지고 있던 모든 디렉터리 inode 비트맵 해제
				//indirect가 존재한다면 indirect의 inode에 들어가서 direct 도 다 해제해주어야함.

				for (k = 0; k < SFS_NDIRECT; k++)
				{
					if (delNode.sfi_direct[k] == 0)
						break;
					deleteBitmap(delNode.sfi_direct[k]);
				}

				//연결된 indirect 노드가 있다면 탐색해서 비트맵 해제
				if (delNode.sfi_indirect != 0)
				{
					//indirect Node를 해제해주고 indirect iNode가 가지고 있던
					//디렉터리 inode 비트맵도 계속 해제해준다.
					deleteBitmap(delNode.sfi_indirect);
					int nodeDirects[SFS_DBPERIDB];

					disk_read(nodeDirects, delNode.sfi_indirect);
					int l;
					for (k = 0; k < SFS_DBPERIDB; k++)
					{
						if (nodeDirects[k] != 0)
						{
							deleteBitmap(nodeDirects[k]);
						}
					}
				}

				//curNode의 사이즈를 줄임
				curNode.sfi_size -= sizeof(struct sfs_dir);
				disk_write(&curNode, sd_cwd.sfd_ino);

				//비트맵에서 해당 노드를 1->0으로 삭제 갱신.
				deleteBitmap(tarNode);

				break;
			}
		}
	}
	//해당 for문을 전부 돌았는데도 path를 찾지 못함.
	if (findPath == 0)
	{
		error_message("rm", path, -1);
		return;
	}
}

void sfs_cpin(const char *local_path, const char *path)
{
	//printf("Not Implemented\n");
	struct sfs_inode si;
	disk_read(&si, sd_cwd.sfd_ino);

	//for consistency
	assert(si.sfi_type == SFS_TYPE_DIR);

	FILE *rfp = fopen(path, "r");
	if (rfp == NULL)
	{
		//바깥의 파일이 존재하지 않을때.
		printf("cpin: can't open %s input file\n", path);
		return;
	}
	fseek(rfp, 0, SEEK_END);
	unsigned file_size = ftell(rfp);
	//printf("filesize : %d\n", file_size);
	if (file_size > SFS_BLOCKSIZE * (SFS_NDIRECT + SFS_DBPERIDB))
	{
		printf("cpin: input file size exceeds the max file size\n");
		return;
	}
	rewind(rfp);

	sfs_touch(local_path);
	int targetNode = searchNode(local_path);
	if (targetNode == -1)
	{
		//touch에서 local_path 이름의 파일이 오류로 인해 만들어지지 않음
		return;
	}

	char buf[SFS_BLOCKSIZE];
	struct sfs_inode newFileInode;
	disk_read(&newFileInode, targetNode);
	if (newFileInode.sfi_size != 0)
	{
		//touch에서 중복되어서 return된 경우 size가 이미 0이 아닐것.
		return;
	}
	while (feof(rfp) == 0)
	{
		int count = fread(buf, sizeof(char), SFS_BLOCKSIZE, rfp);
		// if (feof(rfp))
		// 	break;

		//새로만든 localpath 파일의 targetNode 의 빈 direct 탐색
		//direct 0인 부분 발견했으면 해당 direct 를 가용가능한 비트맵 노드 확인 후 갱신
		//만약 direct가 0인 부분이 없으면 indirect
		// [ indirect가 0이면 새로운 indirect노드 할당(비트맵노드에서 찾아서 indirect를 바꿔주고 int[128] 모두 0으로 초기화해서 해당노드 디스크씀) ]
		//indirect 블럭을 탐색하여 0인 부분을 찾으면 가용가능한 비트맵 노드 확인 후 갱신
		//만약 indirect 128 블럭 모두 0이 아니면 거기서 중단.
		if (newFileInode.sfi_size < SFS_BLOCKSIZE * SFS_NDIRECT)
		{
			//direct 탐색
			int i;
			for (i = 0; i < SFS_NDIRECT; i++)
			{
				if (newFileInode.sfi_direct[i] == 0)
				{
					int newNode = getEmptyNode();
					if (newNode == -1)
					{
						error_message("cpin", local_path, -4);
						return;
					}
					newFileInode.sfi_direct[i] = newNode;
					//buf의 사이즈만큼 targetNode의 size를 += 해줌.
					newFileInode.sfi_size += count;
					disk_write(&newFileInode, targetNode);
					//printf("%d node used size %d plus..\n", newNode, count);
					//해당 newNode 비트맵노드에 buf를 씀
					disk_write(buf, newNode);
					//비트맵노드 사용표시함
					newBitmap(newNode);
					break;
				}
			}
		}
		else
		{
			//indirect 탐색
			if (newFileInode.sfi_indirect == 0)
			{
				//만약 indirect 가 할당안되었다면 먼저 할당.
				int newNode = getEmptyNode();
				if (newNode == -1)
				{
					error_message("cpin", local_path, -4);
					return;
				}
				newFileInode.sfi_indirect = newNode;
				disk_write(&newFileInode, targetNode);
				int tpInodes[SFS_DBPERIDB];
				int i;
				for (i = 0; i < SFS_DBPERIDB; i++)
				{
					tpInodes[i] = 0;
				}
				disk_write(tpInodes, newNode);
				newBitmap(newNode);
			}
			int indirNodes[SFS_DBPERIDB];
			disk_read(indirNodes, newFileInode.sfi_indirect);
			int i, isEmpty = 0;
			for (i = 0; i < SFS_DBPERIDB; i++)
			{
				if (indirNodes[i] == 0)
				{
					//새로운 노드 찾음.
					isEmpty = 1;
					int newNode = getEmptyNode();
					if (newNode == -1)
					{
						error_message("cpin", local_path, -4);
						return;
					}
					indirNodes[i] = newNode;
					newFileInode.sfi_size += count;
					disk_write(&newFileInode, targetNode);
					//printf("%d Indirectnode used size %d plus..\n", newNode, count);
					disk_write(indirNodes, newFileInode.sfi_indirect);
					disk_write(buf, newNode);
					newBitmap(newNode);
					break;
				}
			}
			if (isEmpty == 0)
			{
				//printf("there is no indirBlock128 Empty.. \n");
			}
		}
		memset(buf, 0, SFS_BLOCKSIZE);
	}
	fclose(rfp);
}

void sfs_cpout(const char *local_path, const char *path)
{
	//printf("Not Implemented\n");
	struct sfs_inode si;
	disk_read(&si, sd_cwd.sfd_ino);

	//for consistency
	assert(si.sfi_type == SFS_TYPE_DIR);

	int targetNode = searchNode(local_path);
	if (targetNode == -1)
	{
		//밖으로 빼고자하는 local_path가 없음.
		error_message("cpout", local_path, -1);
		return;
	}

	FILE *tpf = fopen(path, "r");
	if (tpf != NULL)
	{
		//바깥의 파일이 이미 존재할때
		error_message("cpout", path, -6);
		return;
	}

	FILE *rfp = fopen(path, "w");
	// if (rfp != NULL)
	// {

	// }

	char buf[SFS_BLOCKSIZE];
	struct sfs_inode outFileInode;
	disk_read(&outFileInode, targetNode);
	int tarSize = outFileInode.sfi_size;
	int i;
	//내보낼 outfile의 direct부터 모두 탐색
	for (i = 0; i < SFS_NDIRECT; i++)
	{
		if (outFileInode.sfi_direct[i] != 0)
		{
			//해당블럭으로 가서 buf에 받음
			memset(buf, 0, SFS_BLOCKSIZE);
			disk_read(buf, outFileInode.sfi_direct[i]);
			//buf를 외부파일에 씀.
			// printf("%d block access\n", outFileInode.sfi_direct[i]);
			// printf("%s", buf);
			if (tarSize > SFS_BLOCKSIZE)
			{
				tarSize -= SFS_BLOCKSIZE;
				fwrite(buf, sizeof(char), SFS_BLOCKSIZE, rfp);
			}
			else
			{
				fwrite(buf, sizeof(char), tarSize, rfp);
			}
		}
	}
	//indirect가 있으면 탐색
	if (outFileInode.sfi_indirect != 0)
	{
		int tpIndNodes[SFS_DBPERIDB];
		disk_read(tpIndNodes, outFileInode.sfi_indirect);
		int i;
		for (i = 0; i < SFS_DBPERIDB; i++)
		{
			if (tpIndNodes[i] != 0)
			{
				memset(buf, 0, SFS_BLOCKSIZE);
				disk_read(buf, tpIndNodes[i]);
				// printf("%d block access\n", outFileInode.sfi_direct[i]);
				// printf("%s", buf);
				if (tarSize > SFS_BLOCKSIZE)
				{
					tarSize -= SFS_BLOCKSIZE;
					fwrite(buf, sizeof(char), SFS_BLOCKSIZE, rfp);
				}
				else
				{
					fwrite(buf, sizeof(char), tarSize, rfp);
				}
			}
		}
	}

	fclose(rfp);
}

void dump_inode(struct sfs_inode inode)
{
	int i;
	struct sfs_dir dir_entry[SFS_DENTRYPERBLOCK];

	printf("size %d type %d direct ", inode.sfi_size, inode.sfi_type);
	for (i = 0; i < SFS_NDIRECT; i++)
	{
		printf(" %d ", inode.sfi_direct[i]);
	}
	printf(" indirect %d", inode.sfi_indirect);
	printf("\n");

	if (inode.sfi_type == SFS_TYPE_DIR)
	{
		for (i = 0; i < SFS_NDIRECT; i++)
		{
			if (inode.sfi_direct[i] == 0)
				break;
			disk_read(dir_entry, inode.sfi_direct[i]);
			dump_directory(dir_entry);
		}
	}
}

void dump_directory(struct sfs_dir dir_entry[])
{
	int i;
	struct sfs_inode inode;
	for (i = 0; i < SFS_DENTRYPERBLOCK; i++)
	{
		printf("%d %s\n", dir_entry[i].sfd_ino, dir_entry[i].sfd_name);
		disk_read(&inode, dir_entry[i].sfd_ino);
		if (inode.sfi_type == SFS_TYPE_FILE)
		{
			printf("\t");
			dump_inode(inode);
		}
	}
}

void sfs_dump()
{
	// dump the current directory structure
	struct sfs_inode c_inode;

	disk_read(&c_inode, sd_cwd.sfd_ino);
	printf("cwd inode %d name %s\n", sd_cwd.sfd_ino, sd_cwd.sfd_name);
	dump_inode(c_inode);
	printf("\n");
}
