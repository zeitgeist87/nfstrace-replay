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
 *  Created on: 22.03.2013
 *      Author: Andreas Rohner
 */

#ifndef NFSTREE_H_
#define NFSTREE_H_

#include <string>
#include <map>
#include <ctime>
#include <stdint.h>

class NFSTree {
private:
	NFSTree *parent;
	std::string name;
	std::string fh;
	uint64_t size;
	bool created;
	std::map<std::string, NFSTree *> children;
	time_t last_access;

public:
	NFSTree(const std::string &fh, time_t timestamp) :
			parent(0), name(fh), fh(fh), size(0), created(false), last_access(timestamp) {
		if (fh.empty()) {
			throw "NFSTree: Empty fh not allowed";
		}
	}

	NFSTree(const std::string &fh, const std::string &name, time_t timestamp) :
			parent(0), name(name), fh(fh), size(0), created(false), last_access(timestamp)  {
		if (fh.empty() || name.empty()) {
			throw "NFSTree: Empty fh or name not allowed";
		}
	}

	void setLastAccess(time_t timestamp){
		last_access = timestamp;
	}

	time_t getLastAccess(){
		return last_access;
	}

	std::map<std::string, NFSTree *>::iterator children_begin(){
		return children.begin();
	}

	std::map<std::string, NFSTree *>::iterator children_end(){
		return children.end();
	}

	NFSTree *getParent() {
		return parent;
	}
	std::string &getName() {
		return name;
	}
	std::string &getHandle() {
		return fh;
	}
	uint64_t getSize() {
		return size;
	}
	void setSize(uint64_t s) {
		size = s;
	}

	void writeToSize(uint64_t size);

	NFSTree *getChild(const std::string &name);

	void clearEmptyDir();

	void removeChild(NFSTree *child);

	void clearChildren(){
		for(auto i = children.begin(), e = children.end(); i != e; ++i){
			i->second->parent = 0;
		}

		children.clear();
	}

	void addChild(NFSTree *child);

	bool hasChildren(){
		return !children.empty();
	}

	bool isDeletable() {
		return children.empty();
	}

	void deleteChild(NFSTree *child);

	void setName(const std::string &name);

	void setHandle(const std::string &handle) {
		if (handle.empty()) {
			throw "setHandle: Empty fh not allowed";
		}
		this->fh = handle;
	}

	void setParent(NFSTree *p) {
		if (!p) {
			throw "setParent: Tried to set empty parent";
		}
		if (parent)
			parent->removeChild(this);

		parent = p;

		parent->addChild(this);
	}

	std::string calcPath();

	std::string makePath(int mode = 0755);

	bool isCreated() const {
		return created;
	}

	void setCreated(bool created) {
		this->created = created;
	}
};

#endif /* NFSTREE_H_ */
