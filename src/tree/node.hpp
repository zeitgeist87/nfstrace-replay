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

#ifndef TREE_NODE_H_
#define TREE_NODE_H_

#include <cstdint>
#include <cstdio>
#include <ctime>
#include <map>
#include <stdexcept>
#include <string>

#include "display/logger.hpp"
#include "parser/file_handle.hpp"

namespace tree {

class Node {
 private:
  using FileHandle = parser::FileHandle;
  static Logger *logger;
  Node *parent;
  std::string name;
  FileHandle fh;
  uint64_t size;
  bool created;
  bool dir;
  std::map<std::string, Node *> children;
  int64_t last_access;

 public:
  static void setLogger(Logger *l) { logger = l; }

  Node(const FileHandle &fh, int64_t timestamp)
      : parent(nullptr),
        name(fh),
        fh(fh),
        size(0),
        created(false),
        dir(false),
        last_access(timestamp) {
    if (name.empty()) throw NodeException("tree::Node: Empty name not allowed");
  }

  Node(const FileHandle &fh, const std::string &name, int64_t timestamp)
      : parent(nullptr),
        name(name),
        fh(fh),
        size(0),
        created(false),
        dir(false),
        last_access(timestamp) {
    if (fh.empty() || name.empty())
      throw NodeException("tree::Node: Empty name not allowed");
  }

  void setLastAccess(int64_t timestamp) { last_access = timestamp; }
  [[nodiscard]] int64_t getLastAccess() const { return last_access; }

  std::map<std::string, Node *>::iterator children_begin() {
    return children.begin();
  }

  std::map<std::string, Node *>::iterator children_end() {
    return children.end();
  }

  Node *getParent() { return parent; }
  std::string &getName() { return name; }
  FileHandle &getHandle() { return fh; }
  uint64_t getSize() { return size; }
  void setSize(uint64_t s) { size = s; }

  void clearChildren() {
    for (auto &i : children) {
      i.second->parent = nullptr;
    }

    children.clear();
  }

  [[nodiscard]] bool hasChildren() const { return !children.empty(); }
  [[nodiscard]] bool isDeletable() const { return children.empty(); }

  void setHandle(const FileHandle &handle) {
    if (handle.empty()) throw NodeException("tree::Node: Empty fh not allowed");
    fh = handle;
  }

  void setParent(Node *p) {
    if (!p) throw NodeException("tree::Node: Tried to set empty parent");

    if (parent) parent->removeChild(this);

    parent = p;

    parent->addChild(this);
  }

  [[nodiscard]] bool isCreated() const { return created; }
  void setCreated(bool created) { this->created = created; }
  [[nodiscard]] bool isDir() const { return dir; }
  void setDir(bool dir) { this->dir = dir; }

  void setName(const std::string &name);
  void addChild(Node *child);
  void removeChild(Node *child);
  void deleteChild(Node *child);
  [[nodiscard]] Node *getChild(const std::string &name) const;

  [[nodiscard]] bool isChildCreated() const;
  void writeToSize(uint64_t size);
  void clearEmptyDir();
  std::string calcPath();
  std::string makePath(int mode = 0755);

  class NodeException : public std::runtime_error {
    using std::runtime_error::runtime_error;
  };
};

}  // namespace tree

#endif  // TREE_NODE_H_
