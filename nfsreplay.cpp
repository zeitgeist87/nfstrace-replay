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

#include <string>
#include <map>
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
#include "parser.h"
#include "operations.h"
#include "nfstree.h"
#include "gc.h"


using namespace std;


#define NFSREPLAY_USAGE												\
	"Usage: %s [options] [nfs trace file]\n"						\
	"  -b yyyy-mm-dd\tdate to begin the replay\n"					\
	"  -d\t\tenable debug output\n"									\
	"  -D\t\tuse fdatasync\n"									    \
	"  -g\t\tenable gc for unused nodes (default)\n"				\
	"  -G\t\tdisable gc for unused nodes\n"							\
	"  -h\t\tdisplay this help and exit\n"							\
	"  -l yyyy-mm-dd\tstop at limit\n"								\
	"  -s minutes\tinterval to sync according\n"					\
	"\t\tto nfs frame time (defaults to 10)\n"						\
	"  -S\t\tdisable syncing\n"										\
	"  -t\t\tdisplay current time (default)\n"						\
	"  -T\t\tdon't display current time\n"							\
	"  -z\t\twrite only zeros (default is random data)\n"			\

WINDOW *errWin;

void wperror(const char *msg){
	static long i = 0;
	wprintw(errWin, "%ld %s: %s\n", i, msg, strerror(errno));
	wrefresh(errWin);
	i++;
}

static void bailout(){
    void *array[20];
    size_t size;

    // get void*'s for all entries on the stack
    size = backtrace(array, 20);


    backtrace_symbols_fd(array, size, 2);
    exit(1);
}

void handler(int sig) {
    // print out all the frames to stderr
    fprintf(stderr, "Error: signal %d:\n", sig);

    bailout();
}


volatile sig_atomic_t pauseExecution = 0;

void sigint_handler(int sig){
	if(pauseExecution == 0){
		pauseExecution = 1;
	}
}

inline static void processRequest(map<uint32_t, NFSFrame> &transactions,
		NFSFrame &frame) {
	switch (frame.operation) {
	case LOOKUP:
	case CREATE:
	case MKDIR:
	case REMOVE:
	case RMDIR:
		if (!frame.fh.empty() && !frame.name.empty())
			//important: insert does not update value
			transactions[frame.xid] = frame;
		break;
	case ACCESS:
	case GETATTR:
	case WRITE:
	case SETATTR:
		if (!frame.fh.empty())
			transactions[frame.xid] = frame;
		break;
	case RENAME:
		if (!frame.fh.empty() && !frame.fh2.empty() && !frame.name.empty()
				&& !frame.name2.empty())
			transactions[frame.xid] = frame;
		break;
	case LINK:
		if (!frame.fh.empty() && !frame.fh2.empty() && !frame.name.empty())
			transactions[frame.xid] = frame;
		break;
	case SYMLINK:
		if (!frame.fh.empty() && !frame.name.empty() && !frame.name2.empty())
			transactions[frame.xid] = frame;
		break;
	default:
		break;
	}
}

inline static void processResponse(multimap<string, NFSTree *> &fhmap,
		map<uint32_t, NFSFrame> &transactions, const char *randbuf,
		const NFSFrame &res, const bool datasync) {

	auto transIt = transactions.find(res.xid);
	if (res.status != FOK || transIt == transactions.end()) {
		transactions.erase(res.xid);
		return;
	}

	const NFSFrame &req = transIt->second;

	if(res.operation != req.operation || res.time-req.time>GC_MAX_TRANSACTIONTIME){
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

static time_t parseTime(const char *input){
    int year;
    int month;
    int day;

	if(sscanf(input,"%d-%d-%d",&year,&month,&day) == 3){
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

int main(int argc, char **argv) {
	FILE *input;

    signal(SIGSEGV, handler);
	//use c locale important for parsing numbers
	setlocale(LC_ALL, "C");
	int c;
	bool writeZero = false;
	bool displayTime = true;
	bool debugOutput = false;
	int syncMinutes = 10;
	bool noSync = false;
	bool dataSync = false;
	bool enableGC = true;
	time_t startTime = -1;
	time_t endTime = -1;

	while ((c = getopt(argc, argv, "dDzs:ShtTb:l:gG")) != -1) {
		switch (c) {
		case 'z':
			//write only zeros
			writeZero = true;
			break;
		case 's': {
			//sync every x minutes
			int tmp = atoi(optarg);
			if(tmp > 0){
				syncMinutes = tmp;
			}

			break;
		}
		case 'b':
            startTime = parseTime(optarg);
		    break;
		case 'l':
            endTime = parseTime(optarg);
		    break;
		case 'S':
			noSync = true;
			break;
		case 'h':
			printf(NFSREPLAY_USAGE, argv[0]);
			return EXIT_SUCCESS;
		case 't':
			displayTime = true;
			break;
		case 'T':
			//don't display time
			displayTime = false;
			break;
		case 'd':
			debugOutput = true;
			break;
		case 'D':
			dataSync = true;
			break;
		case 'g':
			enableGC = true;
			break;
		case 'G':
			enableGC = false;
			break;
		case '?':
			if (optopt == 's' || optopt == 'b')
				fprintf(stderr, "Option -%c requires an argument.\n", optopt);
			else if (isprint(optopt))
				fprintf(stderr, "Unknown option `-%c'.\n", optopt);
			else
				fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
			return 1;
		default:
			abort();
		}
	}

	if (argc-optind > 0) {
		if (!strcmp(argv[optind], "-")) {
			input = stdin;
		} else {
			char *filename = argv[optind];
			/*
			 * child processes inherit this so
			 * we have to set it before the
			 * call to popen, otherwise Ctrl+C
			 * will be propagated to the child
			 * processes
			 */
			signal(SIGINT, SIG_IGN);

			struct stat st;
			if(stat(filename,&st) == 0){
				if(st.st_mode & S_IFDIR){
					//input is a directory
					string command = "cat ";
					command += filename;
					if(filename[strlen(filename)-1]!='/')
						command += '/';
					//the -i switch tells tar to ignore EOF
					command += "*.tar | tar --to-stdout -i --wildcards -xf - \"*.txt.gz\" | gzip -d -c";
					input = popen(command.c_str(), "r");
				}else{
					//is a file
					char *ext = filename;
					while(*ext!=0)
						ext++;

					while(*ext!='.' && ext>filename)
						ext--;

					if(*ext == '.')
						ext++;

					if(!strcmp("xz",ext)){
						string command = "xz -d -c ";
						command += filename;
						input = popen(command.c_str(), "r");
					}else if(!strcmp("gz",ext)){
						string command = "gzip -d -c ";
						command += filename;
						input = popen(command.c_str(), "r");
					}else if(!strcmp("bz2",ext)){
						string command = "bzip2 -k -d -c ";
						command += filename;
						input = popen(command.c_str(), "r");
					}else{
						input = fopen(filename, "r");
					}
				}
			}else{
				//file does not exist
				fprintf(stderr, "Unable to open '%s': File Not Found\n", filename);
				return EXIT_FAILURE;
			}


			if (NULL == input) {
				fprintf(stderr, "Unable to open '%s': %s\n", filename,
						strerror(errno));
				return EXIT_FAILURE;
			}
			//use Ctrl+C for pause
			signal(SIGINT, sigint_handler);
		}
	} else {
		input = stdin;
	}

	char line[1024];
	char randbuf[RANDBUF_SIZE];
	if(!writeZero){
		FILE *fd = fopen("/dev/urandom", "r");
		if(fread(randbuf, 1, RANDBUF_SIZE, fd)!=RANDBUF_SIZE){
		    perror("ERROR initializing random buffer");
		}
		fclose(fd);
	}else{
		memset(randbuf, 0, RANDBUF_SIZE);
	}

	int sync_fd;
	if ((sync_fd = open(".sync_file_handle", O_RDONLY | O_CREAT, S_IRUSR | S_IWUSR)) == -1){
		perror("ERROR initializing sync file handle");
		return EXIT_FAILURE;
	}

	//map file handles to tree nodes
	multimap<string, NFSTree *> fhmap;
	//map transaction ids to frames
	map<uint32_t, NFSFrame> transactions;

	time_t last_sync = 0;
	time_t last_print = 0;
	time_t last_gc = 0;
	NFSFrame frame;

	//initialize various counter
	unsigned long long lines_read = 0;
	unsigned long long requests_processed = 0;
	unsigned long long responses_processed = 0;
	unsigned long long remove_operations = 0;
	unsigned long long link_operations = 0;
	unsigned long long lookup_operations = 0;
	unsigned long long rename_operations = 0;
	unsigned long long write_operations = 0;
	unsigned long long create_operations = 0;

	initscr();
	refresh();
	curs_set(0);

	WINDOW *timeWin = 0;
	WINDOW *debugWin = 0;

	if(displayTime){
		timeWin = newwin(3, 80, 0, 0);
		box(timeWin, 0, 0);
		mvwprintw(timeWin,0,3, "Current Date");
		wrefresh(timeWin);
	}

	if(debugOutput){
		debugWin = newwin(12, 80, 3, 0);
		box(debugWin, 0, 0);
		mvwprintw(debugWin,0,3, "Debug");
		wrefresh(debugWin);
	}

	errWin = newwin(8, 80-2, debugOutput ? 16 : 4, 1);
	scrollok(errWin, TRUE);
	wrefresh(errWin);

	WINDOW *boxWin = newwin(10, 80, debugOutput ? 15 : 3, 0);
	box(boxWin, '*', '*');
	mvwprintw(boxWin,0,3, "Errors");
	wrefresh(boxWin);

    try{
	    while (fgets(line, sizeof(line), input) != NULL) {
		    parseFrame(frame, line);

		    lines_read++;

		    if (frame.protocol != NOPROT) {
			    //print out current time all 30 seconds
			    if (displayTime && last_print + 30 < frame.time) {
				    char *ts = ctime(&frame.time);
				    ts[strlen(ts) - 1] = 0;

				    if(startTime > frame.time){
				    	mvwprintw(timeWin,1,1, "Fast forward: %s", ts);
				    }else{
				    	mvwprintw(timeWin,1,1, "%s              ", ts);
				    }
					wrefresh(timeWin);

				    if(debugOutput){
						mvwprintw(debugWin,1,1, "Lines read: %lld", lines_read);
						mvwprintw(debugWin,2,1, "Requests processed: %lld", requests_processed);
						mvwprintw(debugWin,3,1, "Responses processed: %lld", responses_processed);
						mvwprintw(debugWin,4,1, "Remove operations: %lld", remove_operations/2);
						mvwprintw(debugWin,5,1, "Link operations: %lld", link_operations/2);
						mvwprintw(debugWin,6,1, "Lookup operations: %lld", lookup_operations/2);
						mvwprintw(debugWin,7,1, "Rename operations: %lld", rename_operations/2);
						mvwprintw(debugWin,8,1, "Write operations: %lld", write_operations/2);
						mvwprintw(debugWin,9,1, "Create operations: %lld", create_operations/2);
						mvwprintw(debugWin,10,1, "In Memory Nodes: %ld      ", fhmap.size());

						wrefresh(debugWin);
				    }

				    last_print = frame.time;
			    }
			    //sync every 10 minutes
			    if (!noSync && last_sync + syncMinutes * 60 < frame.time) {
			    	//sync();
				    if(syncfs(sync_fd) == -1){
				    	wperror("ERROR syncing file system");
				    }
				    last_sync = frame.time;
			    }

			    if(pauseExecution == 1){
			    	mvwprintw(timeWin,0,3, "PAUSE (press any key to continue or Q to quit)");
			    	wrefresh(timeWin);
			    	if(wgetch(stdscr)=='q'){
			    		goto cleanup;
			    	}
					box(timeWin, 0, 0);
					mvwprintw(timeWin,0,3, "Current Date");
					wrefresh(timeWin);
			    	pauseExecution = 0;
			    }

				if (enableGC && ((last_gc + 60*60*12 < frame.time
								&& fhmap.size() > GC_NODE_THRESHOLD)
								|| fhmap.size() > GC_NODE_HARD_THRESHOLD)) {
					wprintw(errWin, "RUNNING GC\n");
					wrefresh(errWin);
					do_gc(fhmap, transactions, frame.time);
					last_gc = frame.time;
				}

			    switch(frame.operation){
			    case REMOVE:
			    case RMDIR:
			    	remove_operations++;
			    	break;
			    case LINK:
			    	link_operations++;
			    	break;
			    case LOOKUP:
			    	lookup_operations++;
			    	break;
			    case RENAME:
			    	rename_operations++;
			    	break;
			    case WRITE:
			    	write_operations++;
			    	break;
			    case CREATE:
			    	create_operations++;
			    	break;
			    default:
			    	break;
			    }

                if(startTime == -1 || startTime < frame.time){
			        if (frame.protocol == C3 || frame.protocol == C2) {
			        	requests_processed++;
				        processRequest(transactions, frame);
			        } else if (frame.protocol == R3 || frame.protocol == R2) {
			        	responses_processed++;
				        processResponse(fhmap, transactions, randbuf, frame, dataSync);
			        }
			    }

                if(endTime != -1 && endTime < frame.time){
                	goto cleanup;
			    }
		    }
	    }
	}catch(char const *msg){
	    printw("%s\n", msg);
	    abort();
	}

	//cleanup
cleanup:
	if(timeWin)
		delwin(timeWin);
	if(debugWin)
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

	return EXIT_SUCCESS;
}
