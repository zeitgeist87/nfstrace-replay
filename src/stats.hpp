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

#ifndef STATS_H_
#define STATS_H_

#include <stdexcept>
#include <string>

#include "parser/frame.hpp"

class Stats {
 public:
  using Frame = parser::Frame;

  unsigned long long linesRead = 0;
  unsigned long long requestsProcessed = 0;
  unsigned long long responsesProcessed = 0;
  unsigned long long removeOperations = 0;
  unsigned long long linkOperations = 0;
  unsigned long long lookupOperations = 0;
  unsigned long long renameOperations = 0;
  unsigned long long writeOperations = 0;
  unsigned long long createOperations = 0;

  void writeReport(const std::string &path) {
    if (path.empty()) return;

    FILE *fd = fopen(path.c_str(), "w");
    if (!fd) throw StatsException("Stats: Unable to open file");

    fprintf(fd, "LinesRead %llu\n", linesRead);
    fprintf(fd, "RequestsProcessed %llu\n", requestsProcessed);
    fprintf(fd, "ResponsesProcessed %llu\n", responsesProcessed);
    fprintf(fd, "RemoveOperations %llu\n", removeOperations);
    fprintf(fd, "LinkOperations %llu\n", linkOperations);
    fprintf(fd, "LookupOperations %llu\n", lookupOperations);
    fprintf(fd, "RenameOperations %llu\n", renameOperations);
    fprintf(fd, "WriteOperations %llu\n", writeOperations);
    fprintf(fd, "CreateOperations %llu\n", createOperations);
    fclose(fd);
  }

  void process(Frame *frame) {
    using namespace parser;

    switch (frame->operation) {
      case REMOVE:
      case RMDIR:
        ++removeOperations;
        break;
      case LINK:
        ++linkOperations;
        break;
      case LOOKUP:
        ++lookupOperations;
        break;
      case RENAME:
        ++renameOperations;
        break;
      case WRITE:
        ++writeOperations;
        break;
      case CREATE:
        ++createOperations;
        break;
      default:
        break;
    }
  }

  class StatsException : public std::runtime_error {
    using std::runtime_error::runtime_error;
  };
};

#endif /* STATS_H_ */
