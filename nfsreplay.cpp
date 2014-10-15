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

#include <cstring>
#include <cstdio>
#include <string>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>
#include <execinfo.h> //needed for bailout
#include <curses.h>

#include "Settings.h"
#include "Stats.h"
#include "Parser.h"
#include "FileSystemMap.h"
#include "TransactionMgr.h"
#include "ConsoleDisplay.h"
#include "Logger.h"

using namespace std;

#define NFSREPLAY_USAGE							\
	"Usage: %s [options] [nfs trace file]\n"			\
	"  -b yyyy-mm-dd\tdate to begin the replay\n"			\
	"  -d\t\tenable debug output\n"					\
	"  -D\t\tuse fdatasync\n"					\
	"  -g\t\tenable gc for unused nodes (default)\n"		\
	"  -G\t\tdisable gc for unused nodes\n"				\
	"  -h\t\tdisplay this help and exit\n"				\
	"  -i\t\tinode test (create empty files)\n"			\
	"  -l yyyy-mm-dd\tstop at limit\n"				\
	"  -r path\twrite report at the end\n"				\
	"  -s minutes\tinterval to sync according\n"			\
	"\t\tto nfs frame time (defaults to 10)\n"			\
	"  -S\t\tdisable syncing\n"					\
	"  -t\t\tdisplay current time (default)\n"			\
	"  -T\t\tdon't display current time\n"				\
	"  -z\t\twrite only zeros (default is random data)\n"

void handler(int sig)
{
	void *array[100];
	size_t size;

	endwin();
	// print out all the frames to stderr
	fprintf(stderr, "Error: signal %d:\n", sig);

	// get void*'s for all entries on the stack
	size = backtrace(array, 100);

	backtrace_symbols_fd(array, size, STDERR_FILENO);

	signal (sig, SIG_DFL);
	raise (sig);
}

static volatile sig_atomic_t pauseExecution = 0;

void sigint_handler(int sig)
{
	if (pauseExecution == 0)
		pauseExecution = 1;
}

static FILE *openInputFile(char *filename)
{
	struct stat st;
	FILE *res = NULL;

	if (!strcmp(filename, "-"))
		return stdin;

	if (stat(filename, &st))
		return NULL;

	/*
	 * child processes inherit this so
	 * we have to set it before the
	 * call to popen, otherwise Ctrl+C
	 * will be propagated to the child
	 * processes
	 */
	signal(SIGINT, SIG_IGN);

	if (st.st_mode & S_IFDIR) {
		//input is a directory
		string command = "cat ";
		command += filename;
		if (filename[strlen(filename) - 1] != '/')
			command += '/';
		//the -i switch tells tar to ignore EOF
		command += "*.tar | tar --to-stdout -i --wildcards -xf "
			   " - \"*.txt.gz\" | gzip -d -c";
		res = popen(command.c_str(), "r");
	} else {
		//is a file
		char *ext = filename;
		while (*ext != 0)
			ext++;

		while (*ext != '.' && ext > filename)
			ext--;

		if (*ext == '.')
			ext++;

		if (!strcmp("xz", ext)) {
			string command = "xz -d -c ";
			command += filename;
			res = popen(command.c_str(), "r");
		} else if (!strcmp("gz", ext)) {
			string command = "gzip -d -c ";
			command += filename;
			res = popen(command.c_str(), "r");
		} else if (!strcmp("bz2", ext)) {
			string command = "bzip2 -k -d -c ";
			command += filename;
			res = popen(command.c_str(), "r");
		} else {
			res = fopen(filename, "r");
		}
	}

	//use Ctrl+C for pause
	signal(SIGINT, sigint_handler);

	return res;
}

static int parseParams(int argc, char **argv, Settings &sett)
{
	int c;

	while ((c = getopt(argc, argv, "dDzs:ShitTb:l:gGr:")) != -1) {
		switch (c) {
		case 'z':
			//write only zeros
			sett.writeZero = true;
			break;
		case 's': {
			//sync every x minutes
			int tmp = atoi(optarg);
			if (tmp > 0) {
				sett.syncMinutes = tmp;
			}
			break;
		}
		case 'b':
			sett.setStartTime(optarg);
			break;
		case 'l':
			sett.setEndTime(optarg);
			break;
		case 'r':
			sett.reportPath = optarg;
			break;
		case 'S':
			sett.noSync = true;
			break;
		case 'h':
			printf(NFSREPLAY_USAGE, argv[0]);
			return EXIT_FAILURE;
		case 'i':
			sett.inodeTest = true;
			break;
		case 't':
			sett.displayTime = true;
			break;
		case 'T':
			//don't display time
			sett.displayTime = false;
			break;
		case 'd':
			sett.debugOutput = true;
			break;
		case 'D':
			sett.dataSync = true;
			break;
		case 'g':
			sett.enableGC = true;
			break;
		case 'G':
			sett.enableGC = false;
			break;
		case '?':
			if (optopt == 's' || optopt == 'b')
				fprintf(stderr,
					"Option -%c requires an argument.\n",
					optopt);
			else if (isprint(optopt))
				fprintf(stderr, "Unknown option `-%c'.\n",
					optopt);
			else
				fprintf(stderr,
					"Unknown option character `\\x%x'.\n",
					optopt);
			return EXIT_FAILURE;
		default:
			return EXIT_FAILURE;
		}
	}

	return EXIT_SUCCESS;
}

int main(int argc, char **argv)
{
	int ret = EXIT_SUCCESS;
	char line[1024];
	FILE *input;
	Settings sett;
	Stats stats;

	signal(SIGSEGV, handler);
	//use c locale important for parsing numbers
	setlocale(LC_ALL, "C");

	if (parseParams(argc, argv, sett) == EXIT_FAILURE)
		return EXIT_FAILURE;

	if (argc - optind > 0) {
		input = openInputFile(argv[optind]);
		if (!input) {
			fprintf(stderr, "Unable to open '%s': %s\n",
				argv[optind], strerror(errno));
			return EXIT_FAILURE;
		}
	} else {
		printf(NFSREPLAY_USAGE, argv[0]);
		return EXIT_FAILURE;
	}


	if ((sett.syncFd = open(".sync_file_handle", O_RDONLY | O_CREAT, S_IRUSR | S_IWUSR)) == -1) {
		perror("ERROR initializing sync file handle");
		return EXIT_FAILURE;
	}

	Logger logger;
	TransactionMgr transMgr(sett, stats, logger);
	ConsoleDisplay disp(sett, stats, transMgr, logger);
	Parser parser;

	try {
		while (fgets(line, sizeof(line), input) != NULL) {
			stats.linesRead++;

			if (!*line)
				continue;

			Frame *frame = parser.parse(line);
			if (!frame)
				continue;

			if (pauseExecution == 1) {
				if (disp.pause()) {
					delete frame;
					break;
				}

				pauseExecution = 0;
			}

			stats.process(frame);
			disp.process(frame);

			if (transMgr.process(frame))
				break;
		}

		stats.writeReport(sett.reportPath);
	} catch (exception &e) {
		disp.destroy();
		fprintf(stderr, "%s\n", e.what());
		ret = EXIT_FAILURE;
	}

	fclose(input);
	close(sett.syncFd);
	remove(".sync_file_handle");

	return ret;
}
