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

#include <string>
#include <map>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <utime.h>
#include "nfstree.h"
#include "operations.h"
#include "parser.h"
#include "nfsreplay.h"
#include "gc.h"

using namespace std;

static void createMoveElement(NFSTree *element, NFSTree *parent, const string &name){

	if(element->isCreated()){
		string oldpath = element->calcPath();
		string newpath = parent->makePath() + '/' + name;

		//move element to new parent
		if (rename(oldpath.c_str(), newpath.c_str())) {
			wperror("ERROR moving");
		}else{
			if(oldpath != newpath)
				remove(oldpath.c_str());
		}
	}

	//move element to new parent
	NFSTree *oldparent = element->getParent();
	if(oldparent)
		oldparent->removeChild(element);
	element->setName(name);
	parent->addChild(element);
	if(oldparent)
		oldparent->clearEmptyDir();
}

void createLookup(multimap<NFS_ID, NFSTree *> &fhmap, const NFSFrame &req,
		const NFSFrame &res) {

	if (NFS_ID_EMPTY(req.fh) || NFS_ID_EMPTY(res.fh) || req.fh == res.fh || req.name.empty()
			|| (res.ftype != REG && res.ftype != DIR) || req.name == "."
			|| req.name == "..")
		return;

	auto it = fhmap.find(req.fh);
	NFSTree *parent;
	if (it != fhmap.end()) {
		parent = it->second;
		parent->setLastAccess(res.time);
	} else {
		//parent is unknown, create new entry
		parent = new NFSTree(req.fh, res.time);
		fhmap.insert(pair<NFS_ID, NFSTree *>(req.fh, parent));
	}

	NFSTree *element = parent->getChild(req.name);
	if (element) {
		if (element->getHandle() != res.fh) {
			it = fhmap.find(res.fh);
			if (res.ftype == DIR && it != fhmap.end()) {
				//no duplicate id's allowed
				//rename to handle name
				createMoveElement(element, parent, element->getHandleName());
				//move in correct element
				element = it->second;
				createMoveElement(element, parent, req.name);

				element->setLastAccess(res.time);
			} else {
				//replace fh of current element
				removeFromMap(fhmap, element);

				element->setHandle(res.fh);
				fhmap.insert(pair<NFS_ID, NFSTree *>(res.fh, element));
			}
		}

		element->setLastAccess(res.time);
	} else {
		//search for real element
		if (res.ftype == DIR) {
			//no links
			it = fhmap.find(res.fh);
			if (it != fhmap.end()) {
				element = it->second;
				createMoveElement(element, parent, req.name);
				element->setLastAccess(res.time);
			} else {
				//element does not exist create it
				element = new NFSTree(res.fh, req.name, res.time);
				fhmap.insert(pair<NFS_ID, NFSTree *>(res.fh, element));
				parent->addChild(element);
			}
		} else {
			//there are links
			it = fhmap.find(res.fh);
			if (it != fhmap.end()) {
				element = it->second;

                if(element->getParent()){
                	//linked file has the same handle but different names
					NFSTree *el = new NFSTree(element->getHandle(), req.name, res.time);
					parent->addChild(el);
					fhmap.insert(pair<NFS_ID, NFSTree *>(element->getHandle(), el));
				}else{
				    //file has no parent move it to new position
					if(element->isCreated()){
						string oldpath = element->calcPath();
						string newpath = parent->makePath() + '/' + req.name;

						if (rename(oldpath.c_str(), newpath.c_str())) {
							wperror("ERROR moving");
						}else{
							if(oldpath != newpath)
								remove(oldpath.c_str());
						}
					}

					element->setName(req.name);
					parent->addChild(element);
				}

                element->setLastAccess(res.time);
			} else {
				//element does not exist create it
				element = new NFSTree(res.fh, req.name, res.time);
				fhmap.insert(pair<NFS_ID, NFSTree *>(res.fh, element));
				parent->addChild(element);
			}
		}
	}
}

void createFile(multimap<NFS_ID, NFSTree *> &fhmap, const NFSFrame &req,
		const NFSFrame &res) {

	if (NFS_ID_EMPTY(req.fh) || NFS_ID_EMPTY(res.fh) || req.fh == res.fh || req.name.empty()
			|| (res.ftype != REG && res.ftype != DIR) || req.name == "."
			|| req.name == "..")
		return;

	auto it = fhmap.find(req.fh);
	NFSTree *parent;
	if (it != fhmap.end()) {
		parent = it->second;
		parent->setLastAccess(res.time);
	} else {
		//parent is unknown, create new entry
		parent = new NFSTree(req.fh, res.time);
		fhmap.insert(pair<NFS_ID, NFSTree *>(req.fh, parent));
		//don't make empty paths
		//parent->makePath();
	}

	NFSTree *element = parent->getChild(req.name);
	if (element) {
		if (element->getHandle() != res.fh) {
			it = fhmap.find(res.fh);
			if (res.ftype == DIR && it != fhmap.end()) {
				//no duplicate id's allowed
				//rename to handle name
				createMoveElement(element, parent, element->getHandleName());
				//move in correct element
				element = it->second;
				createMoveElement(element, parent, req.name);

				element->setLastAccess(res.time);
			} else {
				//replace fh of current element
				removeFromMap(fhmap, element);

				element->setHandle(res.fh);
				fhmap.insert(pair<NFS_ID, NFSTree *>(res.fh, element));
			}
		}

		//write file to new size
		if (res.operation == CREATE) {
			element->setSize(0);
		}

		if (res.ftype != DIR) {
			element->writeToSize(res.size);
		}
		element->setLastAccess(res.time);
	} else {
		//search for real element
		if (res.ftype == DIR) {
			it = fhmap.find(res.fh);
			if (it != fhmap.end()) {
				element = it->second;
				//move element to new parent
				createMoveElement(element, parent, req.name);
				element->setLastAccess(res.time);
			} else {
				//element does not exist create it
				element = new NFSTree(res.fh, req.name, res.time);
				fhmap.insert(pair<NFS_ID, NFSTree *>(res.fh, element));
				parent->addChild(element);
			}
		} else {
			it = fhmap.find(res.fh);
			if (it != fhmap.end()) {
				element = it->second;
				if (element->isCreated()) {
					remove(element->calcPath().c_str());
					element->setCreated(false);
				}

				NFSTree *oldparent = element->getParent();
				if (oldparent)
					oldparent->removeChild(element);
				element->setName(req.name);
				parent->addChild(element);

				//write file to new size
				element->setSize(0);
				element->writeToSize(res.size);

				if (oldparent)
					oldparent->clearEmptyDir();

				element->setLastAccess(res.time);
			} else {
				//element does not exist create it
				element = new NFSTree(res.fh, req.name, res.time);
				fhmap.insert(pair<NFS_ID, NFSTree *>(res.fh, element));
				parent->addChild(element);
				element->writeToSize(res.size);
			}
		}
	}
}

void removeFile(multimap<NFS_ID, NFSTree *> &fhmap, const NFSFrame &req,
		const NFSFrame &res) {
	if (NFS_ID_EMPTY(req.fh) || req.name.empty())
		return;

	auto it = fhmap.find(req.fh);
	if (it != fhmap.end()) {
		NFSTree *dir = it->second;
		NFSTree *element = dir->getChild(req.name);
		if (element && element->isDeletable()) {
			if(element->isCreated()){
				string path = element->calcPath();

				if (remove(path.c_str())) {
					wperror("ERROR removing");
				}
			}

			removeFromMap(fhmap, element);
			dir->deleteChild(element);
			dir->clearEmptyDir();
			dir->setLastAccess(res.time);
		}
	}
}

void writeFile(multimap<NFS_ID, NFSTree *> &fhmap, const NFSFrame &req,
		const NFSFrame &res, const char *randbuf, const bool datasync) {
	if (NFS_ID_EMPTY(req.fh))
		return;

	if (res.ftype == REG) {
		auto it = fhmap.find(req.fh);
		NFSTree *element;
		if (it == fhmap.end()) {
			//element does not exist create it
			element = new NFSTree(req.fh, res.time);
			fhmap.insert(pair<NFS_ID, NFSTree *>(req.fh, element));
		} else {
			element = it->second;
			element->setLastAccess(res.time);
		}

		if(element->getParent() && !element->getParent()->isCreated()){
			element->getParent()->makePath();
		}

		string path = element->calcPath();

		int mode = O_RDWR | O_CREAT;
		if (element->getSize() == 0) {
			mode |= O_TRUNC;
		}

		int fd = open(path.c_str(), mode, S_IRUSR | S_IWUSR);
		if (fd == -1){
			wperror("ERROR writing to file");
		} else {
			element->setCreated(true);
			if (lseek(fd, req.offset, SEEK_SET) == -1) {
				wperror("ERROR writing to file");
			} else {
				uint32_t count = req.count;
				while (count > 0) {
					auto s = min((uint32_t) RANDBUF_SIZE, count);
					write(fd, randbuf, s);
					count -= s;
				}
			}
			if (datasync){
				fdatasync(fd);
			}
			close(fd);
		}
		auto size = element->getSize();
		if (req.offset + req.count > size) {
			element->setSize(req.offset + req.count);
		}
	}
}

void renameFile(multimap<NFS_ID, NFSTree *> &fhmap, const NFSFrame &req,
		const NFSFrame &res) {
	if (NFS_ID_EMPTY(req.fh) || NFS_ID_EMPTY(req.fh2) || req.name.empty()
			|| req.name2.empty() || (req.fh == req.fh2 && req.name == req.name2))
		return;

	auto it = fhmap.find(req.fh);
	auto it2 = fhmap.find(req.fh2);
	if (it != fhmap.end()) {
		NFSTree *dir1 = it->second;

		NFSTree *el = dir1->getChild(req.name);
		if (!el)
			return;

		dir1->setLastAccess(res.time);
		el->setLastAccess(res.time);

		NFSTree *dir2;
		if (it2 != fhmap.end()) {
			dir2 = it2->second;
			dir2->setLastAccess(res.time);
		} else {
			//create new dir
			dir2 = new NFSTree(req.fh2, res.time);
			fhmap.insert(pair<NFS_ID, NFSTree *>(req.fh2, dir2));
		}
		NFSTree *el2 = dir2->getChild(req.name2);
		if ((!el2 || el2->isDeletable()) && el!=el2) {
			if(el->isCreated()){
				string oldpath = el->calcPath();
				string newpath = dir2->makePath() + '/' + req.name2;

				if (rename(oldpath.c_str(), newpath.c_str())) {
					wperror("ERROR renaming");
				} else {
					if(oldpath != newpath)
						remove(oldpath.c_str());
				}
			}

			//target already exists and must be deleted cause it gets overwritten
			if (el2) {
				if(el2->isCreated()){
					el->setCreated(true);
				}
				removeFromMap(fhmap, el2);
				dir2->deleteChild(el2);
			}

			//move src to target
			dir1->removeChild(el);
			el->setName(req.name2);
			dir2->addChild(el);

			dir1->clearEmptyDir();
		}
	}
}

void createLink(multimap<NFS_ID, NFSTree *> &fhmap, const NFSFrame &req,
		const NFSFrame &res) {
	if (NFS_ID_EMPTY(req.fh) || NFS_ID_EMPTY(req.fh2) || req.name.empty())
		return;

	auto it = fhmap.find(req.fh);
	auto it2 = fhmap.find(req.fh2);
	if (it != fhmap.end()) {
		NFSTree *srcfile = it->second;
		srcfile->setLastAccess(res.time);
		NFSTree *targetdir;
		if (it2 != fhmap.end()) {
			targetdir = it2->second;
			targetdir->setLastAccess(res.time);
		} else {
			//create new dir
			targetdir = new NFSTree(req.fh2, res.time);
			fhmap.insert(pair<NFS_ID, NFSTree *>(req.fh2, targetdir));
		}
		NFSTree *element = targetdir->getChild(req.name);
		if ((!element || element->isDeletable()) && element != srcfile) {
			if (element) {
				if (element->isCreated()){
					string path = element->calcPath();
					if (remove(path.c_str())) {
						wperror("ERROR removing");
						return;
					}
				}
				removeFromMap(fhmap, element);
				targetdir->deleteChild(element);
			}
			string oldpath;
			string newpath;
			if(srcfile->isCreated()){
				oldpath = srcfile->calcPath();
				newpath = targetdir->makePath() + '/' + req.name;
			}

			//linked file has the same handle but different names
			NFSTree *el = new NFSTree(srcfile->getHandle(), req.name, res.time);
			targetdir->addChild(el);
			fhmap.insert(pair<NFS_ID, NFSTree *>(srcfile->getHandle(), el));

			if(srcfile->isCreated()){
				if (link(oldpath.c_str(), newpath.c_str())) {
					wperror("ERROR creating link");
				}else{
					el->setCreated(true);
				}
			}
		}
	}
}

void createSymlink(multimap<NFS_ID, NFSTree *> &fhmap,
		const NFSFrame &req, const NFSFrame &res) {
	if (NFS_ID_EMPTY(req.fh) || NFS_ID_EMPTY(res.fh) || req.name.empty() || req.name2.empty())
		return;

	NFSTree *dir;
	auto it = fhmap.find(req.fh);
	if (it != fhmap.end()) {
		dir = it->second;
		dir->setLastAccess(res.time);
	} else {
		//create new dir
		dir = new NFSTree(req.fh, res.time);
		fhmap.insert(pair<NFS_ID, NFSTree *>(req.fh, dir));
	}

	NFSTree *element = dir->getChild(req.name);
	if (!element || element->isDeletable()) {
		if (element) {
			if(element->isCreated()){
				string path = element->calcPath();
				if (remove(path.c_str())) {
					wperror("ERROR removing");
					return;
				}
			}
			removeFromMap(fhmap, element);
			dir->deleteChild(element);
		}

		NFSTree *el = new NFSTree(res.fh, req.name, res.time);
		dir->addChild(el);
		fhmap.insert(pair<NFS_ID, NFSTree *>(res.fh, el));

		if(dir->isCreated()){
			string path = dir->makePath() + '/' + req.name;

			if (symlink(req.name2.c_str(), path.c_str())) {
				wperror("ERROR creating symlink");
			} else {
				el->setCreated(true);
			}
		}
	}
}

void getAttr(multimap<NFS_ID, NFSTree *> &fhmap, const NFSFrame &req,
		const NFSFrame &res) {
	if (NFS_ID_EMPTY(req.fh))
		return;

	auto it = fhmap.find(req.fh);
	if (it != fhmap.end()) {
		NFSTree *element = it->second;
		element->setLastAccess(res.time);

		if(element->isCreated()){
			string path = element->calcPath();
			struct stat buf;

			if (lstat(path.c_str(), &buf)) {
				wperror("ERROR getting attributes");
			}
		}
	}
}

void setAttr(multimap<NFS_ID, NFSTree *> &fhmap, const NFSFrame &req,
		const NFSFrame &res) {
	if (NFS_ID_EMPTY(req.fh))
		return;

	auto it = fhmap.find(req.fh);
	if (it != fhmap.end()) {
		NFSTree *element = it->second;
		element->setLastAccess(res.time);

		if(element->isCreated()){
			string path = element->calcPath();

			if (req.mode
					&& chmod(path.c_str(),
							S_IXUSR | S_IRUSR | S_IWUSR | req.mode)) {
				wperror("ERROR setting attributes");
			}

			if (req.size_occured) {
				element->writeToSize(req.size);
			}

			if (req.atime || req.mtime) {
				struct utimbuf buf;
				buf.actime = req.atime;
				buf.modtime = req.mtime;

				if (utime(path.c_str(), &buf)) {
					wperror("ERROR setting mtime and atime");
				}
			}
		}
	}
}
