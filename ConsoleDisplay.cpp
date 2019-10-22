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

#include "ConsoleDisplay.h"

#include <curses.h>

#include <ctime>

#include "FileSystemTree.h"
#include "Frame.h"
#include "Logger.h"
#include "TransactionMgr.h"

ConsoleDisplay::ConsoleDisplay(Settings &sett, Stats &stats,
                               TransactionMgr &transMgr, Logger &logger)
    : sett(sett), stats(stats), transMgr(transMgr) {
  initscr();
  refresh();
  curs_set(0);

  timeWin = newwin(3, 80, 0, 0);
  box(timeWin, 0, 0);
  mvwprintw(timeWin, 0, 3, "Current Date");
  wrefresh(timeWin);

  if (sett.debugOutput) {
    debugWin = newwin(12, 80, 3, 0);
    box(debugWin, 0, 0);
    mvwprintw(debugWin, 0, 3, "Debug");
    wrefresh(debugWin);
  }

  logWin = newwin(8, 80 - 2, sett.debugOutput ? 16 : 4, 1);
  scrollok(logWin, TRUE);
  wrefresh(logWin);

  boxWin = newwin(10, 80, sett.debugOutput ? 15 : 3, 0);
  box(boxWin, '*', '*');
  mvwprintw(boxWin, 0, 3, "Errors");
  wrefresh(boxWin);

  logger.setDisplay(this);
}

void ConsoleDisplay::process(Frame *frame) {
  int64_t time = frame->time;

  // print out current time all 30 seconds
  if (last_print + 30 < time) {
    if (sett.displayTime) {
      time_t t = time;
      char *ts = ctime(&t);
      ts[strlen(ts) - 1] = 0;

      if (sett.startTime > time)
        mvwprintw(timeWin, 1, 1, "Fast forward: %s", ts);
      else
        mvwprintw(timeWin, 1, 1, "%s              ", ts);

      wrefresh(timeWin);
    }

    if (sett.debugOutput) {
      mvwprintw(debugWin, 1, 1, "Lines read: %lld", stats.linesRead);
      mvwprintw(debugWin, 2, 1, "Requests processed: %lld",
                stats.requestsProcessed);
      mvwprintw(debugWin, 3, 1, "Responses processed: %lld",
                stats.responsesProcessed);
      mvwprintw(debugWin, 4, 1, "Remove operations: %lld",
                stats.removeOperations / 2);
      mvwprintw(debugWin, 5, 1, "Link operations: %lld",
                stats.linkOperations / 2);
      mvwprintw(debugWin, 6, 1, "Lookup operations: %lld",
                stats.lookupOperations / 2);
      mvwprintw(debugWin, 7, 1, "Rename operations: %lld",
                stats.renameOperations / 2);
      mvwprintw(debugWin, 8, 1, "Write operations: %lld",
                stats.writeOperations / 2);
      mvwprintw(debugWin, 9, 1, "Create operations: %lld",
                stats.createOperations / 2);
      mvwprintw(debugWin, 10, 1, "In Memory Nodes: %ld      ", transMgr.size());

      wrefresh(debugWin);
    }

    last_print = time;
  }
}
