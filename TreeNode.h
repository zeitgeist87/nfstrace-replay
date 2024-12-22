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

#ifndef TREENODE_H_
#define TREENODE_H_

#include <string>
#include <map>
#include <ctime>
#include <stdint.h>
#include <cstdio>

#include "Settings.h"
#include "FileHandle.h"
#include "Logger.h"
#include "TraceException.h"

class TreeNode {
private:
	static Logger *logger;
	static Settings *settings;

	TreeNode *parent;
	std::string name;
	FileHandle fh;
	uint64_t size;
	bool created;
	bool dir;
	std::map<std::string, TreeNode *> children;
	int64_t last_access;

	size_t makePathHelper(TreeNode *node, char *buffer, const int mode, Logger *logger);

public:
	static void setLogger(Logger *l) {
		logger = l;
	}

	static void setSettings(Settings *s) {
		settings = s;
	}

	TreeNode(const FileHandle &fh, int64_t timestamp) :
			parent(0), name(fh), fh(fh),  size(0), created(false),
			dir(false), last_access(timestamp)
	{
		if (name.empty())
			throw TraceException("TreeNode: Empty name not allowed");
	}

	TreeNode(const FileHandle &fh, const std::string &name, int64_t timestamp) :
			parent(0), name(name), fh(fh), size(0), created(false),
			dir(false), last_access(timestamp)
	{
		if (fh.empty() || name.empty())
			throw TraceException("TreeNode: Empty name not allowed");
	}

	void setLastAccess(int64_t timestamp) { last_access = timestamp; }
	int64_t getLastAccess() const { return last_access; }

	std::map<std::string, TreeNode *>::iterator children_begin() {
		return children.begin();
	}

	std::map<std::string, TreeNode *>::iterator children_end() {
		return children.end();
	}

	TreeNode *getParent() { return parent; }
	std::string &getName() { return name; }
	FileHandle &getHandle() { return fh; }
	uint64_t getSize() { return size; }
	void setSize(uint64_t s) { size = s; }

	void clearChildren()
	{
		for (auto i = children.begin(), e = children.end(); i != e;
				++i) {
			i->second->parent = 0;
		}

		children.clear();
	}

	bool hasChildren() const { return !children.empty(); }
	bool isDeletable() const { return children.empty();}

	void setHandle(const FileHandle &handle)
	{
		if (handle.empty())
			throw TraceException("TreeNode: Empty fh not allowed");
		fh = handle;
	}

	void setParent(TreeNode *p)
	{
		if (!p)
			throw TraceException("TreeNode: Tried to set empty parent");

		if (parent)
			parent->removeChild(this);

		parent = p;

		parent->addChild(this);
	}

	bool isCreated() const { return created; }
	void setCreated(bool created) { this->created = created; }
	bool isDir() const { return dir; }
	void setDir(bool dir) { this->dir = dir; }

	void setName(const std::string &name);
	void addChild(TreeNode *child);
	void removeChild(TreeNode *child);
	void deleteChild(TreeNode *child);
	TreeNode *getChild(const std::string &name) const;

	bool isChildCreated() const;
	void writeToSize(uint64_t size);
	void clearEmptyDir();
	std::string calcPath();
	std::string makePath(int mode = 0755);

};

#endif /* TREENODE_H_ */
