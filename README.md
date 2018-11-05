# Infinite-File-Curtailer - a program that reads stdin and writes to a fixed size file.

Requires Linux 3.15+ and a filesystem that supports the FALLOC_FL_COLLAPSE_RANGE flag for fallocate.  This is currently ext4 and XFS.  Hopefully more in the future...

----

## Usage

```
./curtail [-s size] <output file>
```

Options:

```
-verbose  enable all debug output (on stderr)
-quiet    disable all non-error output (on stderr)
-size     Maximum size of the output file (ie. 8192, 64K, 10M, 1G, etc) - default is 16K
```

## Example

A typical usage scenario is to capture the output of a program in a file.  Using curtail prevents the program from creating a runaway file that will eventually fill up the filesystem and cause system failure if not handled at the system level.  In the example below, the file my_app_log.txt cannot exceed 2 megabytes in size.

```
./my_app | curtail -s 2M ./my_app_log.txt
```

## Library usage

Curtail can also be integrated directly into an application instead of used on the command line.  Include the file curtail.h and link the application with -lcurtail.  After successfully calling crtl_init, the program's stdout will be directed to the specified file until crtl_term is called.
