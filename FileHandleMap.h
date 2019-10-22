/*
 * FileSystemMap.h
 *
 *  Created on: Jun 7, 2017
 *      Author: andreas
 */

#ifndef FILEHANDLEMAP_H_
#define FILEHANDLEMAP_H_

#include <memory>
#include <unordered_map>

#include "FileHandle.h"
#include "TraceException.h"
#include "TreeNode.h"

class FileHandleMap {
 private:
  std::unordered_multimap<FileHandle, std::unique_ptr<TreeNode>> map;

 public:
  auto begin() { return map.begin(); }

  auto end() { return map.end(); }

  TreeNode *getNode(const FileHandle &fh) {
    auto it = map.find(fh);
    if (it == map.end()) return nullptr;

    return it->second.get();
  }

  std::unique_ptr<TreeNode> removeNode(TreeNode *element) {
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

  void switchNodeHandle(TreeNode *node, const FileHandle &fh) {
    auto it = removeNode(node);
    node->setHandle(fh);
    map.emplace(fh, std::move(it));
  }

  template <class... Args>
  TreeNode *createNode(const FileHandle &fh, Args &&... args) {
    auto it = map.emplace(
        fh, std::make_unique<TreeNode>(fh, std::forward<Args>(args)...));
    return it->second.get();
  }

  template <class... Args>
  TreeNode *getOrCreateNode(const FileHandle &fh, Args &&... args) {
    auto node = getNode(fh);
    return node ? node : createNode(fh, std::forward<Args>(args)...);
  }

  template <class... Args>
  TreeNode *getOrCreateDir(const FileHandle &fh, Args &&... args) {
    auto node = getNode(fh);
    if (!node) {
      node = createNode(fh, std::forward<Args>(args)...);
      node->setDir(true);
    }

    if (!node->isDir())
      throw TraceException("FileSystemMap: Node is not a directory");

    return node;
  }

  FileHandleMap(uint64_t reserve) { map.reserve(reserve); }

  uint64_t size() const { return map.size(); }
};

#endif /* FILEHANDLEMAP_H_ */
