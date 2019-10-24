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

#ifndef FILEHANDLEMAP_H_
#define FILEHANDLEMAP_H_

#include <memory>
#include <unordered_map>

#include "parser/file_handle.hpp"
#include "replay/trace_exception.hpp"
#include "tree/tree_node.hpp"

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
