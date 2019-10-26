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

#include "replay/transaction_mgr.hpp"

#include <unordered_map>
#include <vector>

#include "parser/frame.hpp"
#include "replay/engine.hpp"

using namespace std;
using namespace parser;

namespace replay {

void TransactionMgr::processRequest(std::unique_ptr<const Frame> &&req) {
  switch (req->operation) {
    case LOOKUP:
    case CREATE:
    case MKDIR:
    case REMOVE:
    case RMDIR:
      if (!req->fh.empty() && !req->name.empty()) {
        // Important: insert does not update value
        transactions[req->xid] = std::move(req);
        return;
      }
      break;
    case ACCESS:
    case GETATTR:
    case WRITE:
    case SETATTR:
      if (!req->fh.empty()) {
        transactions[req->xid] = std::move(req);
        ;
        return;
      }
      break;
    case RENAME:
      if (!req->fh.empty() && !req->fh2.empty() && !req->name.empty() &&
          !req->name2.empty()) {
        transactions[req->xid] = std::move(req);
        ;
        return;
      }
      break;
    case LINK:
      if (!req->fh.empty() && !req->fh2.empty() && !req->name.empty()) {
        transactions[req->xid] = std::move(req);
        ;
        return;
      }
      break;
    case SYMLINK:
      if (!req->fh.empty() && !req->name.empty() && !req->name2.empty()) {
        transactions[req->xid] = std::move(req);
        ;
        return;
      }
      break;
    default:
      break;
  }
}

void TransactionMgr::processResponse(std::unique_ptr<const Frame> &&res) {
  auto transIt = transactions.find(res->xid);
  if (transIt == transactions.end()) {
    return;
  }

  auto req = transIt->second.get();

  if (res->status != FOK || res->operation != req->operation ||
      res->time - req->time > GC_MAX_TRANSACTIONTIME) {
    transactions.erase(transIt);
    return;
  }

  engine.process(std::move(transIt->second), std::move(res));
  transactions.erase(transIt);
}

void TransactionMgr::gc(int64_t time) {
  int64_t trans_ko_time = time - GC_MAX_TRANSACTIONTIME;

  // clear up old transactions
  for (auto it = transactions.begin(), e = transactions.end(); it != e;) {
    if (it->second->time < trans_ko_time) {
      transactions.erase(it++);
    } else {
      ++it;
    }
  }
}

int TransactionMgr::process(std::unique_ptr<const Frame> &&frame) {
  int64_t time = frame->time;

  // Sync every 10 minutes
  if (!sett.noSync && last_sync + sett.syncMinutes * 60 < time) {
    if (!engine.sync()) logger.error("Error syncing file system");

    last_sync = time;
  }

  if (sett.enableGC &&
      ((last_gc + 60 * 60 * 12 < time && engine.size() > GC_NODE_THRESHOLD) ||
       engine.size() > GC_NODE_HARD_THRESHOLD)) {
    logger.log("RUNNING GC");

    engine.gc(time);
    gc(time);

    last_gc = time;
  }

  if (sett.startTime < 0 && sett.startAfterDays > 0)
    sett.startTime = time + (sett.startAfterDays * 24 * 60 * 60);

  if (sett.startTime == -1 || sett.startTime < time) {
    if (frame->protocol == C3 || frame->protocol == C2) {
      stats.requestsProcessed++;
      processRequest(std::move(frame));
    } else if (frame->protocol == R3 || frame->protocol == R2) {
      stats.responsesProcessed++;
      processResponse(std::move(frame));
    }
  }

  if (sett.endTime < 0 && sett.endAfterDays > 0) {
    if (sett.startTime > 0)
      sett.endTime = sett.startTime + (sett.endAfterDays * 24 * 60 * 60);
    else
      sett.endTime = time + (sett.endAfterDays * 24 * 60 * 60);
  }

  if (sett.endTime != -1 && sett.endTime < time) return 1;

  return 0;
}

}  // namespace replay
