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

#ifndef CONSOLEDISPLAY_H_
#define CONSOLEDISPLAY_H_

#include <curses.h>

#include <cerrno>

#include "parser/frame.hpp"
#include "settings.hpp"
#include "stats.hpp"

namespace replay {
class TransactionMgr;
}

namespace display {

class Logger;

class ConsoleDisplay {
  Settings &sett;
  Stats &stats;
  replay::TransactionMgr &transMgr;

  WINDOW *timeWin = nullptr;
  WINDOW *debugWin = nullptr;
  WINDOW *boxWin = nullptr;
  WINDOW *logWin = nullptr;

  int64_t last_print = 0;

  using Frame = parser::Frame;

 public:
  ConsoleDisplay(Settings &sett, Stats &stats, replay::TransactionMgr &transMgr,
                 Logger &logger);

  virtual ~ConsoleDisplay() { destroy(); }

  void destroy() {
    // cleanup
    if (timeWin) delwin(timeWin);
    if (debugWin) delwin(debugWin);
    if (logWin) delwin(logWin);
    if (boxWin) {
      delwin(boxWin);
      endwin();
    }

    timeWin = nullptr;
    debugWin = nullptr;
    boxWin = nullptr;
    logWin = nullptr;
  }

  void error(const char *msg) { log("%s: %s\n", msg, strerror(errno)); }

  template <class... Args>
  void log(const char *format, Args &&... args) {
    static long i = 0;
    wprintw(logWin, "%ld ", i);
    wprintw(logWin, format, std::forward<Args>(args)...);
    wrefresh(logWin);
    i++;
  }

  int pause() {
    mvwprintw(timeWin, 0, 3, "PAUSE (press any key to continue or Q to quit)");
    wrefresh(timeWin);
    if (wgetch(stdscr) == 'q') return 1;

    box(timeWin, 0, 0);
    mvwprintw(timeWin, 0, 3, "Current Date");
    wrefresh(timeWin);

    return 0;
  }

  void process(Frame *frame);
};

}  // namespace display

#endif /* CONSOLEDISPLAY_H_ */
