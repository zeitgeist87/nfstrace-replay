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

#include <string>
#include <unordered_map>
#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <signal.h>
#include <curses.h>
#include <locale.h>
#include <execinfo.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>

#include "nfsreplay.h"
#include "nfstree.h"
#include "parser.h"
#include "operations.h"
#include "gc.h"


using namespace std;


#define NFSREPLAY_USAGE							\
	"Usage: %s [options] [nfs trace file]\n"			\
	"  -b yyyy-mm-dd\tdate to begin the replay\n"			\
	"  -d\t\tenable debug output\n"					\
	"  -D\t\tuse fdatasync\n"					\
	"  -g\t\tenable gc for unused nodes (default)\n"		\
	"  -G\t\tdisable gc for unused nodes\n"				\
	"  -h\t\tdisplay this help and exit\n"				\
	"  -l yyyy-mm-dd\tstop at limit\n"				\
	"  -r path\twrite report at the end\n"				\
	"  -s minutes\tinterval to sync according\n"			\
	"\t\tto nfs frame time (defaults to 10)\n"			\
	"  -S\t\tdisable syncing\n"					\
	"  -t\t\tdisplay current time (default)\n"			\
	"  -T\t\tdon't display current time\n"				\
	"  -z\t\twrite only zeros (default is random data)\n"		\

static WINDOW *errWin;

static bool optWriteZero = false;
static bool optDisplayTime = true;
static bool optDebugOutput = false;
static int optSyncMinutes = 10;
static bool optNoSync = false;
static bool optDataSync = false;
static bool optEnableGC = true;
static char *optReportPath = NULL;
static time_t optStartTime = -1;
static time_t optEndTime = -1;
static int optStartAfterDays = -1;
static int optEndAfterDays = -1;

static unsigned long long numLinesRead;
static unsigned long long numRequestsProcessed;
static unsigned long long numResponsesProcessed;
static unsigned long long numRemoveOperations;
static unsigned long long numLinkOperations;
static unsigned long long numLookupOperations;
static unsigned long long numRenameOperations;
static unsigned long long numWriteOperations;
static unsigned long long numCreateOperations;

static time_t last_sync;
static time_t last_print;
static time_t last_gc;

void wperror(const char *msg)
{
	static long i = 0;
	wprintw(errWin, "%ld %s: %s\n", i, msg, strerror(errno));
	wrefresh(errWin);
	i++;
}

void bailout()
{
	void *array[20];
	size_t size;

	// get void*'s for all entries on the stack
	size = backtrace(array, 20);

	backtrace_symbols_fd(array, size, 2);
	exit(1);
}

void handler(int sig)
{
	// print out all the frames to stderr
	fprintf(stderr, "Error: signal %d:\n", sig);

	bailout();
}


static volatile sig_atomic_t pauseExecution = 0;

void sigint_handler(int sig)
{
	if (pauseExecution == 0) {
		pauseExecution = 1;
	}
}

inline static void processRequest(unordered_map<uint32_t, NFSFrame> &transactions,
		NFSFrame &frame)
{
	switch (frame.operation) {
	case LOOKUP:
	case CREATE:
	case MKDIR:
	case REMOVE:
	case RMDIR:
		if (!NFS_ID_EMPTY(frame.fh) && !frame.name.empty())
			//important: insert does not update value
			transactions[frame.xid] = frame;
		break;
	case ACCESS:
	case GETATTR:
	case WRITE:
	case SETATTR:
		if (!NFS_ID_EMPTY(frame.fh))
			transactions[frame.xid] = frame;
		break;
	case RENAME:
		if (!NFS_ID_EMPTY(frame.fh) && !NFS_ID_EMPTY(frame.fh2)
				&& !frame.name.empty() && !frame.name2.empty())
			transactions[frame.xid] = frame;
		break;
	case LINK:
		if (!NFS_ID_EMPTY(frame.fh) && !NFS_ID_EMPTY(frame.fh2)
				&& !frame.name.empty())
			transactions[frame.xid] = frame;
		break;
	case SYMLINK:
		if (!NFS_ID_EMPTY(frame.fh) && !frame.name.empty()
				&& !frame.name2.empty())
			transactions[frame.xid] = frame;
		break;
	default:
		break;
	}
}

inline static void processResponse(unordered_multimap<NFS_ID, NFSTree *> &fhmap,
		unordered_map<uint32_t, NFSFrame> &transactions,
		const char *randbuf, const NFSFrame &res, const bool datasync)
{

	auto transIt = transactions.find(res.xid);
	if (res.status != FOK || transIt == transactions.end()) {
		transactions.erase(res.xid);
		return;
	}

	const NFSFrame &req = transIt->second;

	if (res.operation
			!= req.operation|| res.time-req.time>GC_MAX_TRANSACTIONTIME) {
		transactions.erase(res.xid);
		return;
	}

	switch (res.operation) {
	case LOOKUP:
		createLookup(fhmap, req, res);
		break;
	case CREATE:
	case MKDIR:
		createFile(fhmap, req, res);
		break;
	case REMOVE:
	case RMDIR:
		removeFile(fhmap, req, res);
		break;
	case WRITE:
		writeFile(fhmap, req, res, randbuf, datasync);
		break;
	case RENAME:
		renameFile(fhmap, req, res);
		break;
	case LINK:
		createLink(fhmap, req, res);
		break;
	case SYMLINK:
		createSymlink(fhmap, req, res);
		break;
	case ACCESS:
	case GETATTR:
		getAttr(fhmap, req, res);
		break;
	case SETATTR:
		setAttr(fhmap, req, res);
		break;
	default:
		break;
	}

	if (transIt != transactions.end())
		transactions.erase(transIt);
}

static time_t parseTime(const char *input)
{
	int year;
	int month;
	int day;

	if (sscanf(input, "%d-%d-%d", &year, &month, &day) == 3) {
		struct tm timeinfo;
		memset(&timeinfo, 0, sizeof(struct tm));

		char *tz = getenv("TZ");
		setenv("TZ", "", 1);
		tzset();

		timeinfo.tm_year = year - 1900;
		timeinfo.tm_mon = month - 1;
		timeinfo.tm_mday = day;

		if (tz)
			setenv("TZ", tz, 1);
		else
			unsetenv("TZ");
		tzset();

		return mktime(&timeinfo);
	}
	return -1;
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

static int parseParams(int argc, char **argv)
{
	int c;

	while ((c = getopt(argc, argv, "dDzs:ShtTb:l:gGr:")) != -1) {
		switch (c) {
		case 'z':
			//write only zeros
			optWriteZero = true;
			break;
		case 's': {
			//sync every x minutes
			int tmp = atoi(optarg);
			if (tmp > 0) {
				optSyncMinutes = tmp;
			}
			break;
		}
		case 'b':
			optStartTime = parseTime(optarg);
			if (optStartTime < 0) {
				int tmp = atoi(optarg);
				if (tmp > 0) {
					optStartAfterDays = tmp;
				}
			}
			break;
		case 'l':
			optEndTime = parseTime(optarg);
			if (optEndTime < 0) {
				int tmp = atoi(optarg);
				if (tmp > 0) {
					optEndAfterDays = tmp;
				}
			}
			break;
		case 'r':
			if (optReportPath)
				free(optReportPath);
			optReportPath = strdup(optarg);
			break;
		case 'S':
			optNoSync = true;
			break;
		case 'h':
			printf(NFSREPLAY_USAGE, argv[0]);
			return EXIT_SUCCESS;
		case 't':
			optDisplayTime = true;
			break;
		case 'T':
			//don't display time
			optDisplayTime = false;
			break;
		case 'd':
			optDebugOutput = true;
			break;
		case 'D':
			optDataSync = true;
			break;
		case 'g':
			optEnableGC = true;
			break;
		case 'G':
			optEnableGC = false;
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

	return 0;
}

static inline int processFrame(unordered_multimap<NFS_ID, NFSTree *> &fhmap,
		unordered_map<uint32_t, NFSFrame> &transactions,
		NFSFrame &frame, int sync_fd, char *randbuf, WINDOW *timeWin,
		WINDOW *debugWin)
{
	//print out current time all 30 seconds
	if (optDisplayTime && last_print + 30 < frame.time) {
		char *ts = ctime(&frame.time);
		ts[strlen(ts) - 1] = 0;

		if (optStartTime > frame.time)
			mvwprintw(timeWin, 1, 1, "Fast forward: %s", ts);
		else
			mvwprintw(timeWin, 1, 1, "%s              ", ts);

		wrefresh(timeWin);

		if (optDebugOutput) {
			mvwprintw(debugWin, 1, 1, "Lines read: %lld",
				  numLinesRead);
			mvwprintw(debugWin, 2, 1, "Requests processed: %lld",
				  numRequestsProcessed);
			mvwprintw(debugWin, 3, 1, "Responses processed: %lld",
				  numResponsesProcessed);
			mvwprintw(debugWin, 4, 1, "Remove operations: %lld",
				  numRemoveOperations / 2);
			mvwprintw(debugWin, 5, 1, "Link operations: %lld",
				  numLinkOperations / 2);
			mvwprintw(debugWin, 6, 1, "Lookup operations: %lld",
				  numLookupOperations / 2);
			mvwprintw(debugWin, 7, 1, "Rename operations: %lld",
				  numRenameOperations / 2);
			mvwprintw(debugWin, 8, 1, "Write operations: %lld",
				  numWriteOperations / 2);
			mvwprintw(debugWin, 9, 1, "Create operations: %lld",
				  numCreateOperations / 2);
			mvwprintw(debugWin, 10, 1, "In Memory Nodes: %ld      ",
				  fhmap.size());

			wrefresh(debugWin);
		}

		last_print = frame.time;
	}
	//sync every 10 minutes
	if (!optNoSync && last_sync + optSyncMinutes * 60 < frame.time) {
		//sync();
		if (syncfs(sync_fd) == -1)
			wperror("ERROR syncing file system");

		last_sync = frame.time;
	}

	if (pauseExecution == 1) {
		mvwprintw(timeWin, 0, 3,
			  "PAUSE (press any key to continue or Q to quit)");
		wrefresh(timeWin);
		if (wgetch(stdscr) == 'q')
			return 1;

		box(timeWin, 0, 0);
		mvwprintw(timeWin, 0, 3, "Current Date");
		wrefresh(timeWin);
		pauseExecution = 0;
	}

	if (optEnableGC && ((last_gc + 60 * 60 * 12 < frame.time
				&& fhmap.size() > GC_NODE_THRESHOLD)
				|| fhmap.size() > GC_NODE_HARD_THRESHOLD)) {
		wprintw(errWin, "RUNNING GC\n");
		wrefresh(errWin);
		do_gc(fhmap, transactions, frame.time);
		last_gc = frame.time;
	}

	switch (frame.operation) {
	case REMOVE:
	case RMDIR:
		++numRemoveOperations;
		break;
	case LINK:
		++numLinkOperations;
		break;
	case LOOKUP:
		++numLookupOperations;
		break;
	case RENAME:
		++numRenameOperations;
		break;
	case WRITE:
		++numWriteOperations;
		break;
	case CREATE:
		++numCreateOperations;
		break;
	default:
		break;
	}

	if (optStartTime < 0 && optStartAfterDays > 0)
		optStartTime = frame.time + (optStartAfterDays * 24 * 60 * 60);

	if (optStartTime == -1 || optStartTime < frame.time) {
		if (frame.protocol == C3 || frame.protocol == C2) {
			numRequestsProcessed++;
			processRequest(transactions, frame);
		} else if (frame.protocol == R3 || frame.protocol == R2) {
			numResponsesProcessed++;
			processResponse(fhmap, transactions, randbuf, frame,
					optDataSync);
		}
	}

	if (optEndTime < 0 && optEndAfterDays > 0) {
		if (optStartTime > 0)
			optEndTime = optStartTime + (optEndAfterDays * 24 * 60 * 60);
		else
			optEndTime =frame.time + (optEndAfterDays * 24 * 60 * 60);
	}

	if (optEndTime != -1 && optEndTime < frame.time)
		return 1;
	return 0;
}

static void writeReport() {
	if (optReportPath) {
		FILE *fd = fopen(optReportPath, "w");
		free(optReportPath);
		optReportPath = NULL;

		if (fd) {
			fprintf(fd, "numLinesRead %llu\n", numLinesRead);
			fprintf(fd, "numRequestsProcessed %llu\n",
				numRequestsProcessed);
			fprintf(fd, "numResponsesProcessed %llu\n",
				numResponsesProcessed);
			fprintf(fd, "numRemoveOperations %llu\n",
				numRemoveOperations);
			fprintf(fd, "numLinkOperations %llu\n",
				numLinkOperations);
			fprintf(fd, "numLookupOperations %llu\n",
				numLookupOperations);
			fprintf(fd, "numRenameOperations %llu\n",
				numRenameOperations);
			fprintf(fd, "numWriteOperations %llu\n",
				numWriteOperations);
			fprintf(fd, "numCreateOperations %llu\n",
				numCreateOperations);
			fclose(fd);
		}
	}
}

int main(int argc, char **argv)
{
	FILE *input;

	signal(SIGSEGV, handler);
	//use c locale important for parsing numbers
	setlocale(LC_ALL, "C");

	if (parseParams(argc, argv))
		return EXIT_FAILURE;

	if (argc - optind > 0) {
		input = openInputFile(argv[optind]);
		if (!input) {
			fprintf(stderr, "Unable to open '%s': %s\n",
				argv[optind], strerror(errno));
			return EXIT_FAILURE;
		}
	} else {
		input = stdin;
	}

	char line[1024];
	char randbuf[RANDBUF_SIZE];
	if (!optWriteZero) {
		FILE *fd = fopen("/dev/urandom", "r");
		if (fread(randbuf, 1, RANDBUF_SIZE, fd) != RANDBUF_SIZE) {
			perror("ERROR initializing random buffer");
		}
		fclose(fd);
	} else {
		memset(randbuf, 0, RANDBUF_SIZE);
	}

	int sync_fd;
	if ((sync_fd = open(".sync_file_handle", O_RDONLY | O_CREAT,
			S_IRUSR | S_IWUSR)) == -1) {
		perror("ERROR initializing sync file handle");
		return EXIT_FAILURE;
	}

	//map file handles to tree nodes
	unordered_multimap<NFS_ID, NFSTree *> fhmap;
	fhmap.reserve(GC_NODE_HARD_THRESHOLD);
	//map transaction ids to frames
	unordered_map<uint32_t, NFSFrame> transactions;
	NFSFrame frame;

	initscr();
	refresh();
	curs_set(0);

	WINDOW *timeWin = 0;
	WINDOW *debugWin = 0;

	if (optDisplayTime) {
		timeWin = newwin(3, 80, 0, 0);
		box(timeWin, 0, 0);
		mvwprintw(timeWin, 0, 3, "Current Date");
		wrefresh(timeWin);
	}

	if (optDebugOutput) {
		debugWin = newwin(12, 80, 3, 0);
		box(debugWin, 0, 0);
		mvwprintw(debugWin, 0, 3, "Debug");
		wrefresh(debugWin);
	}

	errWin = newwin(8, 80 - 2, optDebugOutput ? 16 : 4, 1);
	scrollok(errWin, TRUE);
	wrefresh(errWin);

	WINDOW *boxWin = newwin(10, 80, optDebugOutput ? 15 : 3, 0);
	box(boxWin, '*', '*');
	mvwprintw(boxWin, 0, 3, "Errors");
	wrefresh(boxWin);

	try {
		while (fgets(line, sizeof(line), input) != NULL) {
			parseFrame(frame, line);

			numLinesRead++;

			if (frame.protocol != NOPROT) {
				if (processFrame(fhmap, transactions, frame,
						sync_fd, randbuf, timeWin,
						debugWin))
					break;
			}
		}
	} catch (char const *msg) {
		printw("%s\n", msg);
		abort();
	}

	// cleanup
	if (timeWin)
		delwin(timeWin);
	if (debugWin)
		delwin(debugWin);
	delwin(errWin);
	delwin(boxWin);
	endwin();
	fclose(input);
	close(sync_fd);
	remove(".sync_file_handle");

	for (auto it = fhmap.begin(), e = fhmap.end(); it != e; ++it) {
		delete it->second;
	}

	writeReport();

	return EXIT_SUCCESS;
}
