# The Almquist Shell

### Kenneth Almquist (1989)

    The Almquist shell (also known as A Shell, ash and sh) was originally
    Kenneth Almquist's clone of the SVR4-variant of the Bourne shell; it is a
    fast, small, POSIX-compatible Unix shell designed to replace the Bourne
    shell in later BSD distributions.
                                                -- Wikipedia (2013)

## Requirements

    * 32MB RAM

## Usage

Use the ISO containing this file as the boot CD in any popular x86 virtual
machine or emulator.

Attach a fresh hard disk image, and 'checkpoint_save' inside the Shell
will persist the data in /save.

    host# truncate --size=16M save.iso

    host# qemu-system-i386 -cdrom AlmquistShell.iso -hda save.iso

## Credits

### created by *Kenneth Almquist* (1989)
### incorporated into BusyBox (2001)
### combined with Linux for the Century Arcade (2013)

## Copyright and License

The primary content of this archive is copyright 1989 Kenneth Almquist.

This archive also incorporates code from Linux and BusyBox, which are
distributed under the GPLv2 license.  See the LICENSE file for details.

The sources for this archive can be found at
http://src.century-arcade.org/AlmquistShell (which redirects to
https://github.com/century-arcade/src/AlmquistShell).

## Contact

Inquiries sent to almquist-shell@century-arcade.org will be received and
forwarded as appropriate.

