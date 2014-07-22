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

#include <unordered_map>
#include <vector>

#include "Frame.h"
#include "FileSystemMap.h"
#include "TransactionMgr.h"

using namespace std;

void TransactionMgr::processRequest(const Frame *req) {
	switch (req->operation) {
	case LOOKUP:
	case CREATE:
	case MKDIR:
	case REMOVE:
	case RMDIR:
		if (!req->fh.empty() && !req->name.empty()) {
			//important: insert does not update value
			transactions[req->xid] = req;
			return;
		}
		break;
	case ACCESS:
	case GETATTR:
	case WRITE:
	case SETATTR:
		if (!req->fh.empty()) {
			transactions[req->xid] = req;
			return;
		}
		break;
	case RENAME:
		if (!req->fh.empty() && !req->fh2.empty() && !req->name.empty() && !req->name2.empty()) {
			transactions[req->xid] = req;
			return;
		}
		break;
	case LINK:
		if (!req->fh.empty() && !req->fh2.empty() && !req->name.empty()) {
			transactions[req->xid] = req;
			return;
		}
		break;
	case SYMLINK:
		if (!req->fh.empty() && !req->name.empty() && !req->name2.empty()) {
			transactions[req->xid] = req;
			return;
		}
		break;
	default:
		break;
	}

	delete req;
}


void TransactionMgr::processResponse(const Frame *res)
{
	auto transIt = transactions.find(res->xid);
	if (transIt == transactions.end()) {
		delete res;
		return;
	}

	const Frame *req = transIt->second;

	if (res->status != FOK || res->operation != req->operation ||
	   res->time - req->time > GC_MAX_TRANSACTIONTIME) {
		delete req;
		delete res;
		transactions.erase(transIt);
		return;
	}

	fhmap.process(*req, *res);

	delete req;
	delete res;
	transactions.erase(transIt);
}

void TransactionMgr::gc(int64_t time) {
	vector<uint32_t> trans_del_list;
	int64_t trans_ko_time = time - GC_MAX_TRANSACTIONTIME;

	//clear up old transactions
	for (auto it = transactions.begin(), e = transactions.end(); it != e; ++it) {
		const Frame *frame = it->second;
		if (frame->time < trans_ko_time) {
			trans_del_list.push_back(it->first);
		}
	}

	for (auto it = trans_del_list.begin(), e = trans_del_list.end();
			it != e; ++it) {
		transactions.erase(*it);
	}
}
