/*
 * FileSystemMap.h
 *
 *  Created on: Jun 7, 2017
 *      Author: andreas
 */

#ifndef FILEHANDLEMAP_H_
#define FILEHANDLEMAP_H_

#include <unordered_map>
#include <memory>

#include "FileHandle.h"
#include "TreeNode.h"
#include "TraceException.h"

class FileHandleMap {
private:
	// Map file handles to tree nodes
	std::unordered_multimap<FileHandle, std::unique_ptr<TreeNode>> map;

public:
	auto begin() {
		return map.begin();
	}

	auto end() {
		return map.end();
	}

	TreeNode *getNode(const FileHandle &fh) {
		auto it = map.find(fh);
		if (it == map.end())
			return nullptr;

		return it->second.get();
	}

	void replaceHandle(TreeNode *node, const FileHandle &fh) {
		auto it = removeNode(node);
		node->setHandle(fh);
		map.emplace(fh, std::move(it));
	}

	TreeNode *createNode(const FileHandle &fh, int64_t timestamp) {
		auto it = map.emplace(fh, std::make_unique<TreeNode>(fh, timestamp));
		return it->second.get();
	}

	TreeNode *createNode(const FileHandle &fh,  const std::string &name, int64_t timestamp) {
		auto it = map.emplace(fh, std::make_unique<TreeNode>(fh, name, timestamp));
		return it->second.get();
	}

	TreeNode *getOrCreateNode(const FileHandle &fh, int64_t timestamp) {
		auto node = getNode(fh);
		return node ? node : createNode(fh, timestamp);
	}

	TreeNode *getOrCreateNode(const FileHandle &fh, const std::string &name, int64_t timestamp) {
		auto node = getNode(fh);
		return node ? node : createNode(fh, name, timestamp);
	}

	TreeNode *getOrCreateDir(const FileHandle &fh, int64_t timestamp) {
		auto node = getNode(fh);
		if (!node) {
			node = createNode(fh, timestamp);
			node->setDir(true);
		}

		if (!node->isDir())
			throw TraceException("FileSystemMap: Node is not a directory");

		return node;
	}

	TreeNode *getOrCreateDir(const FileHandle &fh, const std::string &name, int64_t timestamp) {
		auto node = getNode(fh);
		if (!node) {
			node = createNode(fh, name, timestamp);
			node->setDir(true);
		}

		if (!node->isDir())
			throw TraceException("FileSystemMap: Node is not a directory");

		return node;
	}

	std::unique_ptr<TreeNode> removeNode(TreeNode *element)
	{
		auto range = map.equal_range(element->getHandle());

		for (auto it = range.first, e = range.second; it != e; ++it) {
			if (it->second.get() == element) {
				// First copy the node
				auto node = std::move(it->second);
				// Second erase the map entry
				map.erase(it);
				return node;
			}
		}

		throw TraceException("FileSystemMap: Element could not be deleted");
	}

	FileHandleMap(uint64_t reserve)
	{
		map.reserve(reserve);
	}

	uint64_t size() const { return map.size(); }
};

#endif /* FILEHANDLEMAP_H_ */
