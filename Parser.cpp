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

#include <map>
#include <string>
#include <cstring>
#include "Parser.h"
#include "TraceException.h"

Frame *Parser::parse(char *line)
{
	char *pos = line;
	char *token = line;
	char *src = 0;
	char *dest = 0;
	char *last_token = 0;
	int count = 0;
	bool eol = false;

	if (!isdigit(*pos))
		return 0;

	Frame *frame = new Frame();

	while (!eol) {
		if (*pos == '"') {
			//if it starts with " ignore space until end "
			pos++;
			token++;
			while (*pos != 0 && *pos != '"')
				pos++;
		} else {
			while (*pos != 0 && *pos != ' ')
				pos++;
		}

		if (*pos == 0)
			eol = true;

		*pos = 0;

		switch (count) {
		case 0:
			frame->time = strtoull(token, NULL, 10);
			break;
		case 1:
			src = token;
			break;
		case 2:
			dest = token;
			break;
		case 4:
			//protocol
			if (*token == 'R') {
				if (token[1] == '2')
					frame->protocol = R2;
				else
					frame->protocol = R3;

				frame->client = parseClientId(dest);
			} else if (*token == 'C') {
				if (token[1] == '2')
					frame->protocol = C2;
				else
					frame->protocol = C3;

				frame->client = parseClientId(src);
			}
			break;
		case 5:
			frame->xid = strtoul(token, NULL, 16);
			break;
		/*case 6:
			 // cannot use opcode cause it is different for R2 R3
			 frame.operation = (Operation) strtoul(token, NULL, 16);
			 break;*/
		case 7:
			frame->operation = parseOpId(token);
			break;
		case 8:
			if (frame->protocol == R2 || frame->protocol == R3) {
				if (!strcmp(token, "OK"))
					frame->status = FOK;
				else
					frame->status = FERROR;
			}
			break;
		}

		if (last_token && (count > 8 || (count == 8 && (frame->protocol == R2 || frame->protocol == R3)))) {
			frame->setAttribute(last_token, token);
		}

		pos++;
		last_token = token;
		token = pos;
		count++;
	}

	if (frame->protocol == NOPROT) {
		delete frame;
		return 0;
	}

	return frame;
}
