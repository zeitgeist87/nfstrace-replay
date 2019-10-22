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

#ifndef PARSER_H_
#define PARSER_H_

#include <cstring>
#include <map>
#include <memory>

#include "Frame.h"

class Parser {
 private:
  struct CompareCStrings {
    bool operator()(const char *lhs, const char *rhs) const {
      return std::strcmp(lhs, rhs) < 0;
    }
  };

  std::map<const char *, OpId, CompareCStrings> opmap;

  uint32_t parseClientId(const char *token) {
    char *pEnd;
    uint32_t first = strtoul(token, &pEnd, 16);
    pEnd++;
    uint32_t second = strtoul(pEnd, nullptr, 16);
    return (first << 16) | second;
  }

  OpId parseOpId(char *op) {
    char *pos = op;
    while (*pos != 0) {
      *pos = tolower(*pos);
      pos++;
    }
    auto it = opmap.find(op);
    if (it != opmap.end()) return it->second;
    return NULLOP;
  }

 public:
  Parser() {
    opmap["null"] = NULLOP;
    opmap["getattr"] = GETATTR;
    opmap["setattr"] = SETATTR;
    opmap["lookup"] = LOOKUP;
    opmap["access"] = ACCESS;
    opmap["read"] = READ;
    opmap["readlink"] = READLINK;
    opmap["write"] = WRITE;
    opmap["create"] = CREATE;
    opmap["mkdir"] = MKDIR;
    opmap["symlink"] = SYMLINK;
    opmap["mknod"] = MKNOD;
    opmap["remove"] = REMOVE;
    opmap["rmdir"] = RMDIR;
    opmap["rename"] = RENAME;
    opmap["link"] = LINK;
    opmap["readdir"] = READDIR;
    opmap["readdirp"] = READDIRPLUS;
    opmap["readdirplus"] = READDIRPLUS;
    opmap["fsstat"] = FSSTAT;
    opmap["fsinfo"] = FSINFO;
    opmap["pathconf"] = PATHCONF;
    opmap["commit"] = COMMIT;
  }

  std::unique_ptr<Frame> parse(char *line);
};

#endif /* PARSER_H_ */
