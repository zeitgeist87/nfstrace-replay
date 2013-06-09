/*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  Created on: 09.06.2013
 *      Author: Andreas Rohner
 */

#ifndef GC_H_
#define GC_H_

#include <map>
#include <ctime>
#include "parser.h"
#include "nfstree.h"

#define GC_NODE_THRESHOLD 1024 * 1024
#define GC_NODE_HARD_THRESHOLD 4 * GC_NODE_THRESHOLD
#define GC_DISCARD_HARD_THRESHOLD 60*5
#define GC_DISCARD_THRESHOLD 60*60*24
#define GC_MAX_TRANSACTIONTIME 5*60

void removeFromMap(std::multimap<std::string, NFSTree *> &fhmap, NFSTree *element);
void do_gc(std::multimap<std::string, NFSTree *> &fhmap, std::map<uint32_t, NFSFrame> &transactions, time_t time);

#endif /* GC_H_ */
