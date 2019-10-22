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

#ifndef FILESYSTEMTREE_H_
#define FILESYSTEMTREE_H_

#include <unistd.h>

#include <string>

#include "FileHandle.h"
#include "FileHandleMap.h"
#include "Frame.h"
#include "Logger.h"
#include "Settings.h"
#include "Stats.h"
#include "TraceException.h"
#include "TreeNode.h"

/*
 * size of the random buffer which is used
 * to fill the created files
 */
#define RANDBUF_SIZE (1024 * 1024)

#define GC_NODE_THRESHOLD (1024 * 1024)
#define GC_NODE_HARD_THRESHOLD (4 * GC_NODE_THRESHOLD)
#define GC_DISCARD_HARD_THRESHOLD (60 * 5)
#define GC_DISCARD_THRESHOLD (60 * 60 * 24)

class FileSystemTree {
 private:
  Settings &sett;
  Stats &stats;
  Logger &logger;

  // Map file handles to tree nodes
  FileHandleMap fhmap;
  char randbuf[RANDBUF_SIZE];

  void createLookup(const Frame &req, const Frame &res);
  void createFile(const Frame &req, const Frame &res);
  void removeFile(const Frame &req, const Frame &res);
  void writeFile(const Frame &req, const Frame &res);
  void renameFile(const Frame &req, const Frame &res);
  void createLink(const Frame &req, const Frame &res);
  void createSymlink(const Frame &req, const Frame &res);
  void getAttr(const Frame &req, const Frame &res);
  void setAttr(const Frame &req, const Frame &res);
  void createMoveElement(TreeNode *element, TreeNode *parent,
                         const std::string &name);
  void createChangeFType(TreeNode *element, FType ftype);

 public:
  FileSystemTree(Settings &sett, Stats &stats, Logger &logger)
      : sett(sett),
        stats(stats),
        logger(logger),
        fhmap(GC_NODE_HARD_THRESHOLD) {
    if (!sett.writeZero) {
      FILE *fd = fopen("/dev/urandom", "r");
      if (fread(randbuf, 1, RANDBUF_SIZE, fd) != RANDBUF_SIZE) {
        perror("FileSystemMap: Failed to initialize random buffer");
      }
      fclose(fd);
    } else {
      memset(randbuf, 0, RANDBUF_SIZE);
    }

    TreeNode::setLogger(&logger);
  }

  uint64_t size() const { return fhmap.size(); }

  int sync() {
    // sync();
    if (syncfs(sett.syncFd) == -1) return 0;
    return 1;
  }

  void gc(int64_t time);

  void process(std::unique_ptr<const Frame> &&reqp,
               std::unique_ptr<const Frame> &&resp) {
    auto &req = *reqp.get();
    auto &res = *resp.get();

    switch (res.operation) {
      case LOOKUP:
        createLookup(req, res);
        break;
      case CREATE:
      case MKDIR:
        createFile(req, res);
        break;
      case REMOVE:
      case RMDIR:
        removeFile(req, res);
        break;
      case WRITE:
        writeFile(req, res);
        break;
      case RENAME:
        renameFile(req, res);
        break;
      case LINK:
        createLink(req, res);
        break;
      case SYMLINK:
        createSymlink(req, res);
        break;
      case ACCESS:
      case GETATTR:
        getAttr(req, res);
        break;
      case SETATTR:
        setAttr(req, res);
        break;
      default:
        break;
    }
  }
};

#endif /* FILESYSTEMTREE_H_ */
