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

#ifndef CNFSPARSE_H_
#define CNFSPARSE_H_

//size of the random buffer which is used
//to fill the created files
#define RANDBUF_SIZE 1024*1024

void wperror(const char *msg);

#define SMALL_NFS_ID 1

#ifdef SMALL_NFS_ID
	#define NFS_ID uint64_t
	#define NFS_ID_R uint64_t
	#define NFS_ID_EMPTY(name) ((name) == 0)
#else
	#define NFS_ID std::string
	#define NFS_ID_R std::string &
	#define NFS_ID_EMPTY(name) ((name).empty())
#endif

#endif /* CNFSPARSE_H_ */
