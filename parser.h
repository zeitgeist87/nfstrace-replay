/*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  Created on: 09.06.2013
 *      Author: Andreas Rohner
 */

#ifndef PARSER_H_
#define PARSER_H_

#include <string>
#include <ctime>
#include <stdint.h>
#include "nfsreplay.h"

enum Protocol{
	NOPROT,R2, C2, R3, C3
};

enum FType {
   NOFILE = 0,
   REG    = 1,
   DIR    = 2,
   BLK    = 3,
   CHR    = 4,
   LNK    = 5,
   SOCK   = 6,
   FIFO   = 7
};

enum Operation{
	NULLOP=0,GETATTR=1,SETATTR=2,LOOKUP=3,ACCESS=4,READ=6,READLINK=5,
	WRITE=7,CREATE=8,MKDIR=9,SYMLINK=10,MKNOD=11,REMOVE=12,
	RMDIR=13,RENAME=14,LINK=15,READDIR=16,READDIRPLUS=17,FSSTAT=18,
	FSINFO=19,PATHCONF=20,COMMIT=21
};

enum Status{
	FSENT=0,FOK,FERROR
};

struct NFSFrame{
	Protocol protocol;
	Operation operation;
	Status status;
	//transaction id
	uint32_t xid;
	time_t time;
	time_t atime;
	time_t mtime;

	uint32_t client;
	bool truncated;


	//attributes
	uint32_t count;
	uint64_t size;
	bool size_occured;
	uint32_t mode;
	uint64_t offset;
	NFS_ID fh;
	std::string name;
	NFS_ID fh2;
	std::string name2;
	FType ftype;

	void clear(){
		ftype=NOFILE;
		protocol=NOPROT;
		operation=NULLOP;
		status=FSENT;
		xid=0;
		time=0;
		atime=0;
		mtime=0;
		client=0;
		truncated=false;
		count=0;
		size=0;
		size_occured=false;
		mode=0;
		offset=0;
		name.clear();

		name2.clear();

		#ifdef SMALL_NFS_ID
			fh = 0;
			fh2 = 0;
		#else
			fh.clear();
			fh2.clear();
		#endif
	}

	NFSFrame(){
		clear();
	}
};

void parseFrame(NFSFrame &frame, char *line);

#endif /* PARSER_H_ */
