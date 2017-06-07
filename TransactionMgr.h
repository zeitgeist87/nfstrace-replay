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

#ifndef TRANSACTIONMGR_H_
#define TRANSACTIONMGR_H_

#include <unordered_map>
#include <memory>

#include "Frame.h"
#include "Settings.h"
#include "Stats.h"
#include "FileSystemMap.h"
#include "Logger.h"

#define GC_MAX_TRANSACTIONTIME		(5 * 60)

class TransactionMgr {
private:
	Settings &sett;
	Stats &stats;
	FileSystemMap fhmap;
	Logger &logger;
	//map transaction ids to frames
	std::unordered_map<uint32_t, std::unique_ptr<const Frame>> transactions;

	int64_t last_sync = 0;
	int64_t last_gc = 0;

	void processRequest(std::unique_ptr<const Frame> &&req);
	void processResponse(std::unique_ptr<const Frame> &&res);

public:
	TransactionMgr(Settings &sett, Stats &stats, Logger &logger) :
			sett(sett), stats(stats), fhmap(sett, stats, logger),
			logger(logger) {

	}

	uint64_t size() {
		return fhmap.size();
	}

	int process(std::unique_ptr<const Frame> &&frame);
	void gc(int64_t time);
};

#endif /* TRANSACTIONMGR_H_ */
