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

#ifndef TRACEEXCEPTION_H_
#define TRACEEXCEPTION_H_

#include <execinfo.h>
#include <exception>

class TraceException : public std::exception {
	std::string message;
public:
	TraceException(std::string msg) : message(msg) {
		void *array[100];
		int size = backtrace(array, 100);

		char **strings = backtrace_symbols(array, size);
		if (strings) {
			for (int i = 1; i < size; ++i) {
				message += '\n';
				message += strings[i];
			}

			// the individual strings do not have to be freed
			free(strings);
		}
	}

	virtual const char* what() const throw() {
		return message.c_str();
	}
};

#endif /* TRACEEXCEPTION_H_ */
