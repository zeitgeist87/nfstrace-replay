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

#include "FileSystemTree.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <utime.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>

#include "FileHandle.h"
#include "Frame.h"
#include "TreeNode.h"

using namespace std;

void FileSystemTree::createMoveElement(TreeNode *element, TreeNode *parent,
                                       const string &name) {
  if (element->isCreated()) {
    string oldpath = element->calcPath();
    string newpath = parent->makePath() + '/' + name;

    // Move element to new parent
    if (rename(oldpath.c_str(), newpath.c_str())) {
      logger.error("ERROR moving");
    } else {
      if (oldpath != newpath) remove(oldpath.c_str());
    }
  }

  // Move element to new parent
  TreeNode *oldparent = element->getParent();
  if (oldparent) oldparent->removeChild(element);
  element->setName(name);
  parent->addChild(element);
  if (oldparent) oldparent->clearEmptyDir();
}

void FileSystemTree::createChangeFType(TreeNode *element, FType ftype) {
  if (ftype == DIR && !element->isDir()) {
    if (element->isCreated()) {
      if (remove(element->calcPath().c_str())) {
        logger.error("ERROR changing type");
      } else {
        element->setCreated(false);
      }
    }

    element->setDir(true);
  } else if (ftype != DIR && element->isDir()) {
    if (element->isCreated()) {
      if (remove(element->calcPath().c_str())) {
        logger.error("ERROR changing type");
      } else {
        element->setCreated(false);
      }
    }

    element->setDir(false);
  }
}

void FileSystemTree::createLookup(const Frame &req, const Frame &res) {
  if (req.fh.empty() || res.fh.empty() || req.fh == res.fh ||
      req.name.empty() || (res.ftype != REG && res.ftype != DIR) ||
      req.name == "." || req.name == "..")
    return;

  auto parent = fhmap.getOrCreateDir(req.fh, res.time);
  auto element = parent->getChild(req.name);
  parent->setLastAccess(res.time);

  if (element) {
    if (element->getHandle() != res.fh) {
      auto tmp = fhmap.getNode(res.fh);
      if (res.ftype == DIR && tmp) {
        // No duplicate ids allowed
        // Rename to handle name
        createMoveElement(element, parent, element->getHandle());
        // Move in correct element
        element = tmp;
        createMoveElement(element, parent, req.name);
      } else {
        // Replace fh of current element
        fhmap.switchNodeHandle(element, res.fh);
      }
    }

    createChangeFType(element, res.ftype);
    element->setLastAccess(res.time);
  } else {
    // Search for real element
    if (res.ftype == DIR) {
      // No links
      element = fhmap.getNode(res.fh);
      if (element) {
        createMoveElement(element, parent, req.name);
        createChangeFType(element, res.ftype);
        element->setLastAccess(res.time);
      } else {
        // Element does not exist create it
        element = fhmap.createNode(res.fh, req.name, res.time);
        element->setDir(true);
        parent->addChild(element);
      }
    } else {
      // There are links
      element = fhmap.getNode(res.fh);
      if (element) {
        if (element->getParent()) {
          // Linked file has the same handle but different names
          auto el = fhmap.createNode(element->getHandle(), req.name, res.time);
          parent->addChild(el);
        } else {
          // File has no parent move it to new position
          if (element->isCreated()) {
            string oldpath = element->calcPath();
            string newpath = parent->makePath() + '/' + req.name;

            if (rename(oldpath.c_str(), newpath.c_str())) {
              logger.error("ERROR moving");
            } else {
              if (oldpath != newpath) remove(oldpath.c_str());
            }
          }

          element->setName(req.name);
          parent->addChild(element);
          createChangeFType(element, res.ftype);
        }

        element->setLastAccess(res.time);
      } else {
        // Element does not exist create it
        element = fhmap.createNode(res.fh, req.name, res.time);
        parent->addChild(element);
      }
    }
  }
}

void FileSystemTree::createFile(const Frame &req, const Frame &res) {
  if (req.fh.empty() || res.fh.empty() || req.fh == res.fh ||
      req.name.empty() || (res.ftype != REG && res.ftype != DIR) ||
      req.name == "." || req.name == "..")
    return;

  TreeNode *parent = fhmap.getOrCreateDir(req.fh, res.time);
  parent->setLastAccess(res.time);

  // Don't make empty paths
  // parent->makePath();

  TreeNode *element = parent->getChild(req.name);
  if (element) {
    if (element->getHandle() != res.fh) {
      auto tmp = fhmap.getNode(res.fh);
      if (res.ftype == DIR && tmp) {
        // No duplicate ids allowed
        // Rename to handle name
        createMoveElement(element, parent, element->getHandle());
        // Move in correct element
        element = tmp;
        createMoveElement(element, parent, req.name);
      } else {
        // Replace fh of current element
        fhmap.switchNodeHandle(element, res.fh);
      }
    }

    // Write file to new size
    if (res.operation == CREATE) element->setSize(0);

    if (res.ftype != DIR) element->writeToSize(res.size);

    createChangeFType(element, res.ftype);
    element->setLastAccess(res.time);
  } else {
    // Search for real element
    if (res.ftype == DIR) {
      element = fhmap.getNode(res.fh);
      if (element) {
        // Move element to new parent
        createMoveElement(element, parent, req.name);
        createChangeFType(element, res.ftype);
        element->setLastAccess(res.time);
      } else {
        // Element does not exist create it
        element = fhmap.createNode(res.fh, req.name, res.time);
        element->setDir(true);
        parent->addChild(element);
      }
    } else {
      element = fhmap.getNode(res.fh);
      if (element) {
        if (element->isCreated()) {
          if (remove(element->calcPath().c_str()))
            logger.error("ERROR creating element");
          else
            element->setCreated(false);
        }

        TreeNode *oldparent = element->getParent();
        if (oldparent) oldparent->removeChild(element);
        element->setName(req.name);
        parent->addChild(element);

        createChangeFType(element, res.ftype);

        // Write file to new size
        element->setSize(0);
        element->writeToSize(res.size);

        if (oldparent) oldparent->clearEmptyDir();

        element->setLastAccess(res.time);
      } else {
        // Element does not exist create it
        element = fhmap.createNode(res.fh, req.name, res.time);
        parent->addChild(element);
        element->writeToSize(res.size);
      }
    }
  }
}

void FileSystemTree::removeFile(const Frame &req, const Frame &res) {
  if (req.fh.empty() || req.name.empty()) return;

  auto dir = fhmap.getNode(req.fh);
  if (!dir) return;

  TreeNode *element = dir->getChild(req.name);
  if (element && element->isDeletable()) {
    if (element->isCreated()) {
      string path = element->calcPath();

      if (remove(path.c_str())) logger.error("ERROR removing");
    }

    dir->deleteChild(element);
    dir->clearEmptyDir();
    dir->setLastAccess(res.time);
    fhmap.removeNode(element);
  }
}

void FileSystemTree::writeFile(const Frame &req, const Frame &res) {
  if (req.fh.empty() || res.ftype != REG) return;

  auto element = fhmap.getOrCreateNode(req.fh, res.time);
  element->setLastAccess(res.time);

  if (element->getParent() && !element->getParent()->isCreated())
    element->getParent()->makePath();

  string path = element->calcPath();

  int mode = O_RDWR | O_CREAT;
  if (element->getSize() == 0) mode |= O_TRUNC;

  if (req.offset + req.count > element->getSize())
    element->setSize(req.offset + req.count);

  int i, fd = -1;
  ssize_t ret = 0;

  // try three times to open the file and then give up
  for (i = 0; i < 3; ++i) {
    if ((fd = open(path.c_str(), mode, S_IRUSR | S_IWUSR)) != -1 ||
        errno != ENOSPC)
      break;
    sleep(10);
  }

  if (fd == -1) {
    logger.error("ERROR opening file");
  } else if (sett.inodeTest) {
    element->setCreated(true);
    ftruncate(fd, element->getSize());
    close(fd);
  } else {
    element->setCreated(true);
    if (lseek(fd, req.offset, SEEK_SET) == -1) {
      logger.error("ERROR seeking file");
    } else {
      uint32_t count = req.count;
      while (count > 0) {
        auto s = min((uint32_t)RANDBUF_SIZE, count);

        // try three times to write the file and then give up
        for (i = 0; i < 3; ++i) {
          if ((ret = write(fd, randbuf, s)) > -1 || errno != ENOSPC) break;
          sleep(10);
        }
        if (ret == -1) {
          logger.error("ERROR writing file");
          break;
        }
        count -= ret;
      }
    }
    if (sett.dataSync) {
      fdatasync(fd);
    }
    close(fd);
  }
}

void FileSystemTree::renameFile(const Frame &req, const Frame &res) {
  if (req.fh.empty() || req.fh2.empty() || req.name.empty() ||
      req.name2.empty() || (req.fh == req.fh2 && req.name == req.name2))
    return;

  auto dir1 = fhmap.getNode(req.fh);
  if (!dir1) return;

  TreeNode *el = dir1->getChild(req.name);
  if (!el) return;

  auto dir2 = fhmap.getOrCreateDir(req.fh2, res.time);

  dir1->setLastAccess(res.time);
  dir2->setLastAccess(res.time);
  el->setLastAccess(res.time);

  TreeNode *el2 = dir2->getChild(req.name2);
  if ((!el2 || el2->isDeletable()) && el != el2) {
    if (el->isCreated()) {
      string oldpath = el->calcPath();
      string newpath = dir2->makePath() + '/' + req.name2;

      if (rename(oldpath.c_str(), newpath.c_str())) {
        logger.error("ERROR renaming");
      } else {
        if (oldpath != newpath) remove(oldpath.c_str());
      }
    }

    // target already exists and must be deleted cause it gets overwritten
    if (el2) {
      if (el2->isCreated()) {
        el->setCreated(true);
      }
      dir2->deleteChild(el2);
      fhmap.removeNode(el2);
    }

    // move src to target
    dir1->removeChild(el);
    el->setName(req.name2);
    dir2->addChild(el);

    dir1->clearEmptyDir();
  }
}

void FileSystemTree::createLink(const Frame &req, const Frame &res) {
  if (req.fh.empty() || req.fh2.empty() || req.name.empty()) return;

  auto srcfile = fhmap.getNode(req.fh);
  if (!srcfile) return;

  srcfile->setLastAccess(res.time);

  auto targetdir = fhmap.getOrCreateDir(req.fh2, res.time);
  targetdir->setLastAccess(res.time);

  TreeNode *element = targetdir->getChild(req.name);
  if ((!element || element->isDeletable()) && element != srcfile) {
    if (element) {
      if (element->isCreated()) {
        string path = element->calcPath();
        if (remove(path.c_str())) {
          logger.error("ERROR removing");
          return;
        }
      }
      targetdir->deleteChild(element);
      fhmap.removeNode(element);
    }
    string oldpath;
    string newpath;
    if (srcfile->isCreated()) {
      oldpath = srcfile->calcPath();
      newpath = targetdir->makePath() + '/' + req.name;
    }

    // Linked file has the same handle but different names
    auto el = fhmap.createNode(srcfile->getHandle(), req.name, res.time);
    targetdir->addChild(el);

    if (srcfile->isCreated()) {
      if (link(oldpath.c_str(), newpath.c_str()) && errno != EEXIST)
        logger.error("ERROR creating link");
      else
        el->setCreated(true);
    }
  }
}

void FileSystemTree::createSymlink(const Frame &req, const Frame &res) {
  if (req.fh.empty() || res.fh.empty() || req.name.empty() || req.name2.empty())
    return;

  auto dir = fhmap.getOrCreateDir(req.fh, res.time);
  dir->setLastAccess(res.time);

  auto element = dir->getChild(req.name);
  if (element && !element->isDeletable()) return;

  if (element) {
    if (element->isCreated()) {
      string path = element->calcPath();
      if (remove(path.c_str())) {
        logger.error("ERROR removing");
        return;
      }
    }
    dir->deleteChild(element);
    fhmap.removeNode(element);
  }

  auto el = fhmap.createNode(res.fh, req.name, res.time);
  dir->addChild(el);

  if (dir->isCreated()) {
    string path = dir->makePath() + '/' + req.name;

    if (symlink(req.name2.c_str(), path.c_str()) && errno != EEXIST)
      logger.error("ERROR creating symlink");
    else
      el->setCreated(true);
  }
}

void FileSystemTree::getAttr(const Frame &req, const Frame &res) {
  if (req.fh.empty()) return;

  auto element = fhmap.getNode(req.fh);
  if (!element) return;

  element->setLastAccess(res.time);

  if (element->isCreated()) {
    string path = element->calcPath();
    struct stat buf;

    if (lstat(path.c_str(), &buf)) logger.error("ERROR getting attributes");
  }
}

void FileSystemTree::setAttr(const Frame &req, const Frame &res) {
  if (req.fh.empty()) return;

  auto element = fhmap.getNode(req.fh);
  if (!element) return;

  element->setLastAccess(res.time);

  if (element->isCreated()) {
    string path = element->calcPath();

    if (req.mode && chmod(path.c_str(), S_IXUSR | S_IRUSR | S_IWUSR | req.mode))
      logger.error("ERROR setting attributes");

    /* too many wrong values in the traces e.g. > 20 TB */
    /*if (req.size_occured) {
     element->writeToSize(req.size);
     }*/

    if (req.atime || req.mtime) {
      struct utimbuf buf;
      buf.actime = req.atime;
      buf.modtime = req.mtime;

      if (utime(path.c_str(), &buf))
        logger.error("ERROR setting mtime and atime");
    }
  }
}

static bool recursive_tree_gc(TreeNode *element, set<TreeNode *> &del_list,
                              time_t ko_time) {
  if (!element || element->isCreated() || element->getLastAccess() >= ko_time)
    return false;

  if (del_list.count(element)) return true;

  // Test children
  for (auto it = element->children_begin(), e = element->children_end();
       it != e; ++it) {
    if (recursive_tree_gc(it->second, del_list, ko_time) == false) return false;
  }

  // Found an empty not created old element
  del_list.insert(element);
  element->clearChildren();
  return true;
}

void FileSystemTree::gc(int64_t time) {
  set<TreeNode *> del_list;
  time_t ko_time = fhmap.size() > GC_NODE_HARD_THRESHOLD
                       ? time - GC_DISCARD_HARD_THRESHOLD
                       : time - GC_DISCARD_THRESHOLD;

  // Clear up old unused file handlers
  for (auto it = fhmap.begin(), e = fhmap.end(); it != e; ++it) {
    TreeNode *element = it->second.get();
    if (recursive_tree_gc(element, del_list, ko_time) && element->getParent()) {
      element->getParent()->removeChild(element);
    }
  }

  for (auto it = del_list.begin(), e = del_list.end(); it != e; ++it) {
    TreeNode *element = *it;
    fhmap.removeNode(element);
  }
}
