# Ogniewski Scaler

TODO Description

## Dependencies
On Ubuntu 20.04.4 LTS on ARM64 in a VM on an M1 MacBook Air, the following installs were needed
```bash
$ sudo apt install gimp
$ sudo apt install build-essential
$ sudo apt install libgimp2.0-dev
```
Installing GIMP via snap can have some problems with accessing the plugins folder.


Based on: GIMP Plug-In Template
=====================

Copyright (C) 2000-2003  Michael Natterer <mitch@gimp.org>


This package is an empty plug-in for GIMP 2.0. It features all the
nice things a good GIMP plug-in should have:

- The source is separated in main/interface/render files (core/ui separation).
- It's prepared for I18N.
- It installs it's own help files.
- Full autoconf/automake support.
- A dialog with examples of all libgimp widgets.

Basically everything a Plug-In author should ever need except
her own image manupulation routines.


To build and install it, just ...

	./configure
	make
	make install

 ... and it's there.

And don't forget to remove (or update if really necessary) comments
for translators in po/README.translators.

The default license for the gimp-plugin-template is an ultra-permissive
XFree-style license to encourage the use of the template for GPL /or/
non-GPL plugins.  If you wish to use a different license for your plugin
(e.g. GPL) then you may change the license block in the 'COPYING' file
and the source to your favourite license.


Happy GIMPing,
--Mitch