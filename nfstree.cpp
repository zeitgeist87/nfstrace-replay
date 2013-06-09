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
#include <cstdio>
#include <stdint.h>
#include <sys/stat.h>
#include "nfstree.h"
#include "cnfsparse.h"

void NFSTree::writeToSize(uint64_t size) {
	auto curr = getSize();
	if (curr == size && created)
		return;

	const char *mode = "rb+";
	if (size < curr || !created) {
		mode = "w";
	}

	if(parent && !parent->isCreated()){
		parent->makePath();
	}

	setSize(size);

	FILE *fd = fopen(calcPath().c_str(), mode);
	if (fd != NULL) {
		if (size > 0) {
			if (fseek(fd, size - 1, SEEK_SET) != 0) {
				wperror("ERROR writing to file");
			} else {
				fwrite("w", 1, 1, fd);
			}
		}
		fclose(fd);
	}
	created = true;
}

NFSTree *NFSTree::getChild(const std::string &name) {
	auto it = children.find(name);
	if (it == children.end())
		return 0;
	return it->second;
}

void NFSTree::clearEmptyDir(){
	if(children.empty() && isCreated()){
		if(remove(calcPath().c_str())){
			wperror("ERROR recursive remove");
		}else{
			setCreated(false);
			if(parent)
				parent->clearEmptyDir();
		}
	}
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
	NFSTree *node = this;
	std::string path = node->name;
	node = node->parent;
	while (node) {
		path = node->name + '/' + path;
		node = node->parent;
	}
	return path;
}

std::string NFSTree::makePath(int mode) {
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
}
