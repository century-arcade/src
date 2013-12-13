### mkizo

Creates a hybrid .iso/.zip image from the specified source path.  The resulting .izo can be mounted or unzip'ed.

#### usage

`mkizo -o <target.izo> [-v] [-b <boot-image>] <source-path>`

metadata is given via environment variables, i.e. `bibliographic_id` and
`expiration_date`.  Use -v (verbose) to print the list of undefined metadata.

The target image is deleted if it already exists.

#### features

The target image:

* is a [.zip archive](http://www.pkware.com/documents/casestudies/APPNOTE.TXT)
* is also an [ISO9660 filesystem](http://www.ecma-international.org/publications/files/ECMA-ST/Ecma-119.pdf)
* is [El Torito bootable](http://download.intel.com/support/motherboards/desktop/sb/specscdrom.pdf)
* can have up to 256 files
* is an exact multiple of 1MB (for compatibility with VirtualBox)

#### history

Originally, iso2zip took an already-made .iso and inserted .zip local file
headers in the leftover space where possible.  But
[genisoimage](https://wiki.debian.org/genisoimage) won't obey -sort order, and
.izo images don't need that many features, so mkizo was born.

#### author

* 2013: mkizo was written by [Saul Pwanson](saul@pwanson.com) and donated to the [Century Arcade](century-arcade.org).
