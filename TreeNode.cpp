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

#include <string>
#include <map>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <sys/stat.h>
#include <cerrno>
#include "TreeNode.h"
#include "Logger.h"
#include "TraceException.h"

Logger *TreeNode::logger;

void TreeNode::writeToSize(uint64_t size)
{
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

	if (parent && !parent->isCreated()) {
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

	if (fd == -1) {
		logger->error("ERROR opening file");
		return;
	}

	created = true;

	if (ftruncate(fd, size)) {
		if (errno == EPERM) {
			if (lseek(fd, size - 1, SEEK_SET) == -1) {
				logger->error("ERROR seeking file");
			} else {
				if (write(fd, "w", 1) != 1)
					logger->error("ERROR writing file");
			}
		} else {
			logger->error("ERROR truncating file");
		}
	}

	close(fd);
}

TreeNode *TreeNode::getChild(const std::string &name) const
{
	auto it = children.find(name);
	if (it == children.end())
		return 0;
	return it->second;
}

bool TreeNode::isChildCreated() const
{
	for (auto it = children.begin(), e = children.end(); it != e; ++it) {
		if (it->second->isCreated())
			return true;
	}
	return false;
}

void TreeNode::clearEmptyDir()
{
	TreeNode *el = this;
	do {
		if (el->isCreated()
				&& (!el->hasChildren() || !el->isChildCreated())) {
			if (remove(el->calcPath().c_str())) {
				logger->error("ERROR recursive remove");
				break;
			} else {
				el->setCreated(false);
				el = el->getParent();
			}
		} else {
			break;
		}
	} while (el);
}

void TreeNode::removeChild(TreeNode *child)
{
	if (!child || this == child || fh == child->fh)
		throw TraceException("TreeNode: Wrong parameter");

	auto it = children.find(child->name);
	if (it == children.end() || it->second != child)
		throw TraceException("TreeNode: Cannot find element");

	children.erase(it);
	child->parent = 0;
}

void TreeNode::addChild(TreeNode *child)
{
	if (!child || this == child || fh == child->fh)
		throw TraceException("TreeNode: Wrong parameter");

	auto it = children.find(child->name);
	if (it != children.end()) {
		if (it->second == child) {
			//nothing to do
			return;
		}
		throw TraceException("TreeNode: Element with same name already present");
	}

	if (child->parent)
		child->parent->removeChild(child);

	children.insert(std::pair<std::string, TreeNode *>(child->name, child));
	child->parent = this;
}

void TreeNode::deleteChild(TreeNode *child)
{
	if (!child || !child->children.empty())
		throw TraceException("TreeNode: Wrong parameter or children not empty");

	removeChild(child);
}

void TreeNode::setName(const std::string &name)
{
	if (name.empty())
		throw TraceException("TreeNode: Empty name not allowed");

	if (name == this->name)
		return;

	if (parent)
		throw TraceException("TreeNode: Cannot rename an element if it is attached to a parent");

	this->name = name;
}

std::string TreeNode::calcPath()
{
	char buffer[4096];
	char *pos = buffer + 4096;
	TreeNode *node = this;

	do {
		pos = pos - node->getName().size();
		if (pos <= buffer)
			throw TraceException("TreeNode: Path too long");

		memcpy(pos, node->getName().c_str(), node->getName().size());
		node = node->getParent();

		--pos;
		*pos = '/';
	} while (node);

	++pos;
	return std::string(pos, (size_t) (buffer + 4096 - pos));
}

static size_t makePathHelper(TreeNode *node, char *buffer, const int mode, Logger *logger)
{
	if (!node)
		return 0;

	size_t pos = makePathHelper(node->getParent(), buffer, mode, logger);

	if (pos) {
		buffer[pos] = '/';
		++pos;
	}

	if (pos + node->getName().size() > 4096)
		throw TraceException("TreeNode: Path too long");

	memcpy(buffer + pos, node->getName().c_str(), node->getName().size());

	pos += node->getName().size();

	buffer[pos] = 0;

	if (!node->isCreated() && mkdir(buffer, mode) && errno != EEXIST)
		logger->error("ERROR creating directory");
	else
		node->setCreated(true);

	return pos;
}

std::string TreeNode::makePath(const int mode)
{
	if (isCreated())
		return calcPath();

	char buffer[4096];

	size_t len = makePathHelper(this, buffer, mode, logger);
	return std::string(buffer, len);
}
