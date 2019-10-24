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

#ifndef LOGGER_H_
#define LOGGER_H_

#include <cerrno>
#include <cstdio>

#include "console_display.hpp"

class Logger {
  ConsoleDisplay *disp = nullptr;

 public:
  void setDisplay(ConsoleDisplay *disp) { this->disp = disp; }
  void error(const char *msg) {
    if (disp)
      disp->log("%s: %s\n", msg, strerror(errno));
    else
      perror(msg);
  }
  void log(const char *msg) {
    if (disp)
      disp->log("%s\n", msg);
    else
      puts(msg);
  }
};

#endif /* LOGGER_H_ */
