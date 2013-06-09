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

#include <map>
#include <string>
#include <cstring>
#include "parser.h"

using namespace std;

struct CompareCStrings
{
    bool operator()(const char* lhs, const char* rhs) const {
        return std::strcmp(lhs, rhs) < 0;
    }
};

static map<const char *, Operation, CompareCStrings> opmap;

inline static uint32_t parseClientId(const char *token){
	char *pEnd;
	uint32_t first=strtoul(token,&pEnd,16);
	pEnd++;
	uint32_t second=strtoul(pEnd,NULL,16);
	return (first << 16) | second;
}

inline static void initOpmap() {
	opmap["null"] = NULLOP;
	opmap["getattr"] = GETATTR;
	opmap["setattr"] = SETATTR;
	opmap["lookup"] = LOOKUP;
	opmap["access"] = ACCESS;
	opmap["read"] = READ;
	opmap["readlink"] = READLINK;
	opmap["write"] = WRITE;
	opmap["create"] = CREATE;
	opmap["mkdir"] = MKDIR;
	opmap["symlink"] = SYMLINK;
	opmap["mknod"] = MKNOD;
	opmap["remove"] = REMOVE;
	opmap["rmdir"] = RMDIR;
	opmap["rename"] = RENAME;
	opmap["link"] = LINK;
	opmap["readdir"] = READDIR;
	opmap["readdirp"] = READDIRPLUS;
	opmap["readdirplus"] = READDIRPLUS;
	opmap["fsstat"] = FSSTAT;
	opmap["fsinfo"] = FSINFO;
	opmap["pathconf"] = PATHCONF;
	opmap["commit"] = COMMIT;
}

inline static Operation parseOp(char *op){

	if(opmap.size()==0)
		initOpmap();

	char *pos=op;
	while(*pos!=0){
		*pos=tolower(*pos);
		pos++;
	}
	auto it=opmap.find(op);
	if(it!=opmap.end())
		return it->second;
	return NULLOP;
}

void parseFrame(NFSFrame &frame, char *line) {
	char *pos = line;
	char *token = line;
	char *src = NULL;
	char *dest = NULL;
	char *last_token = NULL;
	int count = 0;
	bool eol = false;

	frame.clear();

	if (!isdigit(*pos))
		return;

	while (!eol) {
		if (*pos == '"') {
			//if it starts with " ignore space until end "
			pos++;
			token++;
			while (*pos != 0 && *pos != '"')
				pos++;
		} else {
			while (*pos != 0 && *pos != ' ')
				pos++;
		}

		if (*pos == 0)
			eol = true;

		*pos = 0;

		switch (count) {
		case 0:
			frame.time = strtoull(token, NULL, 10);
			break;
		case 1:
			src = token;
			break;
		case 2:
			dest = token;
			break;
		case 4:
			//protocol
			if (*token == 'R') {
				if (token[1] == '2') {
					frame.protocol = R2;
				} else {
					frame.protocol = R3;
				}

				frame.client = parseClientId(dest);
			} else if (*token == 'C') {
				if (token[1] == '2') {
					frame.protocol = C2;
				} else {
					frame.protocol = C3;
				}
				frame.client = parseClientId(src);
			}
			break;
		case 5:
			frame.xid = strtoul(token, NULL, 16);
			break;
		/*case 6:
		 	 // cannot use opcode cause it is different for R2 R3
			frame.operation = (Operation) strtoul(token, NULL, 16);
			break;*/
		case 7:
			frame.operation = parseOp(token);
			break;
		case 8:
			if (frame.protocol == R2 || frame.protocol == R3) {
				if (!strcmp(token, "OK")) {
					frame.status = FOK;
				} else {
					frame.status = FERROR;
				}
			}
			break;
		}

		if (last_token
				&& (count > 8
						|| (count == 8
								&& (frame.protocol == R2 || frame.protocol == R3)))) {

			if (!frame.count
					&& (!strcmp(last_token, "count")
							|| !strcmp(last_token, "tcount"))) {
				frame.count = strtoul(token, NULL, 16);
			} else if (frame.name.empty()
					&& (!strcmp(last_token, "name") || !strcmp(last_token, "fn"))) {
				frame.name = token;
			} else if (!frame.size_occured && !strcmp(last_token, "size")) {
				//only read first size
				frame.size_occured = true;
				frame.size = strtoull(token, NULL, 16);
			} else if (frame.ftype == NOFILE && !strcmp(last_token, "ftype")) {
				//only read first ftype
				frame.ftype = (FType) atoi(token);
			} else if (!strcmp(token, "LONGPKT")) {
				frame.truncated = true;
			} else if (!frame.offset
					&& (!strcmp(last_token, "off")
							|| !strcmp(last_token, "offset"))) {
				frame.offset = strtoull(token, NULL, 16);
			} else if (frame.fh.empty() && !strcmp(last_token, "fh")) {
				frame.fh = token;
			} else if (frame.fh2.empty() && !strcmp(last_token, "fh2")) {
				frame.fh2 = token;
			} else if (frame.name2.empty()
					&& (!strcmp(last_token, "fn2")
							|| !strcmp(last_token, "name2")
							|| !strcmp(last_token, "sdata"))) {
				frame.name2 = token;
			} else if (!frame.mode && !strcmp(last_token, "mode")) {
				frame.mode = 0x1FF & strtoul(token, NULL, 16);
			} else if (!frame.atime && !strcmp(last_token, "atime")) {
				frame.atime = strtoull(token, NULL, 10);
			} else if (!frame.mtime && !strcmp(last_token, "mtime")) {
				frame.mtime = strtoull(token, NULL, 10);
			}
		}

		pos++;
		last_token = token;
		token = pos;
		count++;
	}
}
