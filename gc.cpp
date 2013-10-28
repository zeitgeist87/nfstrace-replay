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

#include <set>
#include <vector>
#include <ctime>
#include <unordered_map>
#include "gc.h"
#include "nfsreplay.h"
#include "parser.h"
#include "nfstree.h"

using namespace std;

void removeFromMap(std::unordered_multimap<NFS_ID, NFSTree *> &fhmap, NFSTree *element){
	auto range = fhmap.equal_range(element->getHandle());

	for (auto it = range.first, e = range.second; it != e; ++it) {
		if (it->second == element) {
			fhmap.erase(it);
			return;
		}
	}

	throw "removeFromMap: Element could not be deleted";
}

bool recursive_tree_gc(NFSTree *element, set<NFSTree *> &del_list, time_t ko_time){
	if(!element || element->isCreated() || element->getLastAccess() >= ko_time)
		return false;

	if(del_list.count(element))
		return true;

	//test children
	for (auto it = element->children_begin(), e = element->children_end(); it != e; ++it) {
		if(recursive_tree_gc(it->second, del_list, ko_time)==false)
			return false;
	}

	//found an empty not created old element
	del_list.insert(element);
	element->clearChildren();
	return true;
}

void do_gc(std::unordered_multimap<NFS_ID, NFSTree *> &fhmap, std::unordered_map<uint32_t, NFSFrame> &transactions, time_t time){
	set<NFSTree *> del_list;
	vector<uint32_t> trans_del_list;
	time_t ko_time = fhmap.size()> GC_NODE_HARD_THRESHOLD ? time - GC_DISCARD_HARD_THRESHOLD : time - GC_DISCARD_THRESHOLD;
	time_t trans_ko_time = time - GC_MAX_TRANSACTIONTIME;

	//clear up old unused file handlers
	for (auto it = fhmap.begin(), e = fhmap.end(); it != e; ++it) {
		NFSTree *element = it->second;
		if(recursive_tree_gc(element, del_list, ko_time) && element->getParent()){
			element->getParent()->removeChild(element);
		}
	}

	for(auto it = del_list.begin(), e = del_list.end(); it != e; ++it){
		NFSTree *element = *it;
		removeFromMap(fhmap, element);
		delete element;
	}

	//clear up old transactions
	for(auto it = transactions.begin(), e = transactions.end(); it != e; ++it){
		NFSFrame &frame = it->second;
		if(frame.time < trans_ko_time){
			trans_del_list.push_back(it->first);
		}
	}

	for(auto it = trans_del_list.begin(), e = trans_del_list.end(); it != e; ++it){
		transactions.erase(*it);
	}
}
