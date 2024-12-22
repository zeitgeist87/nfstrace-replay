/*
 * nfstrace-replay - Small command line tool to replay file system traces
 * Copyright (C) 2014  Andreas Rohner
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef FRAME_H_
#define FRAME_H_

#include <string>
#include <cstdint>
#include "FileHandle.h"

enum Protocol {
	NOPROT, R2, C2, R3, C3
};

enum FType {
	NOFILE = 0,
	REG = 1,
	DIR = 2,
	BLK = 3,
	CHR = 4,
	LNK = 5,
	SOCK = 6,
	FIFO = 7
};

enum OpId {
	NULLOP = 0,
	GETATTR = 1,
	SETATTR = 2,
	LOOKUP = 3,
	ACCESS = 4,
	READ = 6,
	READLINK = 5,
	WRITE = 7,
	CREATE = 8,
	MKDIR = 9,
	SYMLINK = 10,
	MKNOD = 11,
	REMOVE = 12,
	RMDIR = 13,
	RENAME = 14,
	LINK = 15,
	READDIR = 16,
	READDIRPLUS = 17,
	FSSTAT = 18,
	FSINFO = 19,
	PATHCONF = 20,
	COMMIT = 21
};

enum Status {
	FSENT = 0, FOK, FERROR
};

class Frame {
private:
	bool size_occured;
public:
	Protocol protocol;
	OpId operation;
	Status status;
	//transaction id
	uint32_t xid;
	int64_t time;
	int64_t atime;
	int64_t mtime;

	uint32_t client;
	bool truncated;

	//attributes
	uint32_t count;
	uint64_t size;
	uint32_t mode;
	uint64_t offset;
	FileHandle fh;
	FileHandle fh2;
	std::string name;
	std::string name2;
	FType ftype;

	Frame() {
		ftype = NOFILE;
		protocol = NOPROT;
		operation = NULLOP;
		status = FSENT;
		xid = 0;
		time = 0;
		atime = 0;
		mtime = 0;
		client = 0;
		truncated = false;
		count = 0;
		size = 0;
		size_occured = false;
		mode = 0;
		offset = 0;
		name.clear();
		name2.clear();
		fh.clear();
		fh2.clear();
	}

	void setAttribute(const char *name, char *token) {
		if (!count && (!strcmp(name, "count")
				|| !strcmp(name, "tcount"))) {
			count = strtoul(token, NULL, 16);
		} else if (this->name.empty() && (!strcmp(name, "name")
				|| !strcmp(name, "fn"))) {
			this->name = token;
		} else if (!size_occured && !strcmp(name, "size")) {
			//only read first size
			size_occured = true;
			size = strtoull(token, NULL, 16);
		} else if (ftype == NOFILE && !strcmp(name, "ftype")) {
			//only read first ftype
			ftype = (FType) atoi(token);
		} else if (!strcmp(token, "LONGPKT")) {
			truncated = true;
		} else if (!offset && (!strcmp(name, "off")
				|| !strcmp(name, "offset"))) {
			offset = strtoull(token, NULL, 16);
		} else if (fh.empty() && !strcmp(name, "fh")) {
			fh = token;
		} else if (fh2.empty() && !strcmp(name, "fh2")) {
			fh2 = token;
		} else if (name2.empty() && (!strcmp(name, "fn2")
				|| !strcmp(name, "name2")
				|| !strcmp(name, "sdata"))) {
			name2 = token;
		} else if (!mode && !strcmp(name, "mode")) {
			mode = 0x1FF & strtoul(token, NULL, 16);
		} else if (!atime && !strcmp(name, "atime")) {
			atime = strtoull(token, NULL, 10);
		} else if (!mtime && !strcmp(name, "mtime")) {
			mtime = strtoull(token, NULL, 10);
		}
	}
};

#endif /* FRAME_H_ */
