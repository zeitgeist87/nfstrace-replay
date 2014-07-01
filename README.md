nfstrace-replay
===============
A simple tool to replay NFS Traces for file system testing.
It can directly read the format of the traces,
from here: http://iotta.snia.org/tracetypes/2

It can also read better compressed files: *.xz, *.bz2

## Usage

```
./nfsreplay -h
Usage: ./nfsreplay [options] [nfs trace file]
  -b yyyy-mm-dd	date to begin the replay
  -d		enable debug output
  -D		use fdatasync
  -g		enable gc for unused nodes (default)
  -G		disable gc for unused nodes
  -h		display this help and exit
  -l yyyy-mm-dd	stop at limit
  -r path	write report at the end
  -s minutes	interval to sync according
		to nfs frame time (defaults to 10)
  -S		disable syncing
  -t		display current time (default)
  -T		don't display current time
  -z		write only zeros (default is random data)
```

Most options have sensible defaults, so an example of its usage would be:

```
./nfsreplay -r report.txt -d "traces/lair62b.txt.xz"
```

It is also possible to read in a whole directory of *.tar files, which
is the typical format of the legacy traces on http://iotta.snia.org/

```
./nfsreplay -r report2.txt -d "traces/home02"
```
