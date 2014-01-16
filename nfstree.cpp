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
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <stdint.h>
#include <sys/stat.h>
#include <cerrno>
#include "nfstree.h"
#include "nfsreplay.h"

void NFSTree::writeToSize(uint64_t size) {
	auto curr = getSize();

	if (created) {
		if (curr == size)
			return;

		if (truncate(calcPath().c_str(), size) == 0)
			return;
	}

	int mode = O_RDWR | O_CREAT;
	if (size < curr) {
		mode |= O_TRUNC;
	}

	if(parent && !parent->isCreated()){
		parent->makePath();
	}

	setSize(size);


	int i, fd = -1;
	//try three times to open the file and then give up
	for (i = 0; i < 3; ++i) {
		if ((fd = open(calcPath().c_str(), mode, S_IRUSR | S_IWUSR)) != -1 || errno != ENOSPC)
			break;
		sleep(10);
	}

	if (fd == -1){
		wperror("ERROR opening file");
		return;
	}

	created = true;

	if (ftruncate(fd, size)) {
		if (errno == EPERM) {
			if (lseek(fd, size - 1, SEEK_SET) == -1) {
				wperror("ERROR seeking file");
			} else {
				if (write(fd, "w", 1) != 1)
					wperror("ERROR writing file");
			}
		} else {
			wperror("ERROR truncating file");
		}
	}

	close(fd);
}



NFSTree *NFSTree::getChild(const std::string &name) {
	auto it = children.find(name);
	if (it == children.end())
		return 0;
	return it->second;
}

bool NFSTree::isChildCreated() {
	for(auto it = children.begin(), e = children.end(); it != e; ++it){
		if (it->second->isCreated())
			return true;
	}
	return false;
}

void NFSTree::clearEmptyDir(){
	NFSTree *el = this;
	do{
		if(el->isCreated() && (!el->hasChildren() || !el->isChildCreated())){
			if(remove(el->calcPath().c_str())){
				wperror("ERROR recursive remove");
				break;
			}else{
				el->setCreated(false);
				el = el->getParent();
			}
		} else {
			break;
		}
	}while(el);
}

void NFSTree::removeChild(NFSTree *child) {
	if (!child || this == child || fh == child->fh)
		throw "removeChild: Wrong parameter";

	auto it = children.find(child->name);
	if (it == children.end() || it->second != child)
		throw "removeChild: Cannot find element";

	children.erase(it);
	child->parent = 0;
}

void NFSTree::addChild(NFSTree *child) {
	if (!child || this == child || fh == child->fh)
		throw "addChild: Wrong parameter";

	auto it = children.find(child->name);
	if (it != children.end()) {
		if (it->second == child) {
			//nothing to do
			return;
		}
		throw "addChild: Element with same name already present";
	}

	if (child->parent)
		child->parent->removeChild(child);

	children.insert(std::pair<std::string, NFSTree *>(child->name, child));
	child->parent = this;
}

void NFSTree::deleteChild(NFSTree *child) {
	if (!child || !child->children.empty())
		throw "deleteChild: Wrong parameter or children not empty";

	removeChild(child);

	delete child;
}

void NFSTree::setName(const std::string &name) {
	if (name.empty()) {
		throw "setName: Empty name not allowed";
	}

	if (name == this->name)
		return;

	if (parent) {
		throw "setName: Cannot rename an element if it is attached to a parent";
	}

	this->name = name;
}

std::string NFSTree::calcPath() {
	char buffer[4096];
	char *pos = buffer + 4096;
	NFSTree *node = this;

	do {
		pos = pos - node->getName().size();
		if (pos <= buffer)
			throw "calcPath: Path too long";

		memcpy(pos, node->getName().c_str(), node->getName().size());
		node = node->getParent();

		--pos;
		*pos = '/';
	}while(node);

	++pos;
	return std::string(pos, (size_t)(buffer + 4096 - pos));
}

/*std::string NFSTree::calcPath() {
	NFSTree *node = this;
	std::string path = node->name;
	node = node->parent;
	while (node) {
		path = node->name + '/' + path;
		node = node->parent;
	}
	return path;
}*/

static size_t makePathHelper(NFSTree *node, char *buffer, int mode, int print){
	if (!node)
		return 0;

	size_t pos = makePathHelper(node->getParent(), buffer, mode, print);

	if (pos) {
		buffer[pos] = '/';
		++pos;
	}

	if (pos + node->getName().size() > 4096)
		throw "makePath: Path too long";

	memcpy(buffer + pos, node->getName().c_str(), node->getName().size());

	pos += node->getName().size();

	buffer[pos] = 0;

	if (!node->isCreated() && mkdir(buffer, mode)) {
		wperror("ERROR creating directory");
	} else {
		node->setCreated(true);
	}

	return pos;
}

std::string NFSTree::makePath(int mode) {
	if (isCreated())
		return calcPath();

	char buffer[4096];

	size_t len = makePathHelper(this, buffer, mode, false);
	return std::string(buffer, len);
}

/*std::string NFSTree::makePath(int mode) {
	if (created)
		return calcPath();
	std::string path = name;
	if (parent)
		path = parent->makePath(mode) + '/' + path;

	if (mkdir(path.c_str(), mode)) {
		wperror("ERROR creating directory");
	}
	created = true;
	return path;
}*/
