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

#ifndef OPERATIONS_H_
#define OPERATIONS_H_

#include <string>
#include <map>
#include "nfstree.h"
#include "parser.h"


void createLookup(std::multimap<std::string, NFSTree *> &fhmap, const NFSFrame &req,
		const NFSFrame &res);

void createFile(std::multimap<std::string, NFSTree *> &fhmap, const NFSFrame &req,
		const NFSFrame &res);

void removeFile(std::multimap<std::string, NFSTree *> &fhmap, const NFSFrame &req,
		const NFSFrame &res);

void writeFile(std::multimap<std::string, NFSTree *> &fhmap, const NFSFrame &req,
		const NFSFrame &res, const char *randbuf, const bool datasync);

void renameFile(std::multimap<std::string, NFSTree *> &fhmap, const NFSFrame &req,
		const NFSFrame &res);

void createLink(std::multimap<std::string, NFSTree *> &fhmap, const NFSFrame &req,
		const NFSFrame &res);

void createSymlink(std::multimap<std::string, NFSTree *> &fhmap,
		const NFSFrame &req, const NFSFrame &res);

void getAttr(std::multimap<std::string, NFSTree *> &fhmap, const NFSFrame &req,
		const NFSFrame &res);

void setAttr(std::multimap<std::string, NFSTree *> &fhmap, const NFSFrame &req,
		const NFSFrame &res);


#endif /* OPERATIONS_H_ */
