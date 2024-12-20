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

#ifndef FILEHANDLE_H_
#define FILEHANDLE_H_

#include <cstring>
#include <cstdint>
#include <cstdio>
#include <string>
#include <functional>

class FileHandleInt {
private:
	uint64_t handle = 0;
public:
	bool operator== (const FileHandleInt &other) const {
		return handle == other.handle;
	}

	bool operator!= (const FileHandleInt &other) const {
		return handle != other.handle;
	}

	FileHandleInt &operator=(char *token) {
		/*
		 * the nfs_handle is usually 64 bytes long,
		 * but most of it is constant, because the device id
		 * and other constants rarely change
		 *
		 * the constant parts always add up to the same value
		 * the variable parts like inode number should fit into 64 bits
		 */
		char tmp;
		char *tmp2, *tokptr = token;
		uint64_t res = 0;

		while (*tokptr) {
			tmp = tokptr[16];
			tokptr[16] = 0;
			res += strtoull(tokptr, &tmp2, 16);
			tokptr[16] = tmp;
			tokptr = tmp2;
		}

		// res added up to 0 by accident, but 0 is not allowed
		if (!res && tokptr != token)
			res = 1;

		handle = res;

		return *this;
	}

	bool empty() const { return handle == 0; }
	void clear() { handle = 0; }

	/*
	 * conversion operator to std::string
	 */
	operator std::string() const {
		char hexString[2 * sizeof(uint64_t) + 3];
		// returns decimal value of hex
		sprintf(hexString,"0x%lX", handle);
		return hexString;
	}

	friend struct std::hash<FileHandleInt>;
};

namespace std {
template<>
struct hash<FileHandleInt> {
	std::size_t operator()(FileHandleInt const& h) const {
		return std::hash<uint64_t>()(h.handle);
	}
};
}

//typedef std::string FileHandle;
typedef FileHandleInt FileHandle;


#endif /* FILEHANDLE_H_ */
