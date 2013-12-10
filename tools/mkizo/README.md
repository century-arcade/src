### mkizo

Creates a dual-format .iso/.zip image from the specified source path.  The resulting .izo can be mounted or unzip'ed.

#### usage

`mkizo -o <target.izo> [-f] [-v] [-b <boot-image>] [-c <comment-file>] <source-path>`

metadata is given via environment variables, i.e. `bibliographic_id` and
`expiration_date`.  Use -v (verbose) to print the list of undefined metadata.

If the `<target.izo>` file is a block device, the .izo will take up the entire 
device.

##### options

* -f: force overwrite if file exists
* -v: verbose
* -b: `<boot-image>` is relative to the `<source-path>`.
* -c: include a .zip comment at the end of the .izo

#### features

The target image:

* is an [ISO9660 filesystem](http://www.ecma-international.org/publications/files/ECMA-ST/Ecma-119.pdf)
* is also an uncompressed [.zip archive](http://www.pkware.com/documents/casestudies/APPNOTE.TXT)
* is an [El Torito bootable CD/DVD](http://download.intel.com/support/motherboards/desktop/sb/specscdrom.pdf)
* can have up to 256 files
* is an exact multiple of 1MB (for compatibility with VirtualBox)

#### restrictions

* does not support Rock Ridge/Joliet or any other extensions

#### history

Originally, iso2zip took an already-made .iso and inserted .zip local file
headers in the leftover space where possible.  But
[genisoimage](https://wiki.debian.org/genisoimage) won't obey -sort order, and
.izo images don't need that many features, so mkizo was born.

#### acknowledgments

* 2013: mkizo was written by [Saul Pwanson](saul@pwanson.com) and donated to the [Century Arcade](http://century-arcade.org).

#### todo

* add option for whitelist file; use `ISO_HIDDEN` and do not include in .zip if not in whitelist
* remove leading slash from zip filename for root files.  directory handling may be considerably wonky.
* file metadata needs to use real dates.  use info from stat(2) or allow to be set from a metadata file.
* add option for file to include as System Area (to be bootable as a hard disk image too)


