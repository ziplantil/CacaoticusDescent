# Cacaoticus Descent
_Cacaoticus Descent_ is a version of _[Chocolate Descent](https://github.com/InsanityBringer/ChocolateDescent)_ that adds
localization/translation support to the game.

In order to run Cacaoticus Descent, you need to have a language
.HOG. See the _data_ folder.

To select a language, create a file called `LANGUAGE.CFG` in
the game directory and enter the language .HOG name
(without the file extension).

## TODO
* Descent 2 (currently only D1 supported)
* Test larger fonts (such as in multiplayer etc.)
* Add RTL support

The original _Chocolate Descent_ description follows.

## Chocolate Descent 

A barebones software-rendered focused port of the Descent game.

Building:
See building.txt for details. The project has been successfully built 
on Windows using Visual Studio 2019 and Linux with gnu make and gcc

Running:
Chocolate Descent needs game data to run. At the moment, Chocolate Descent
will only work with HOG and PIG files from Descent's PC commercial release, 
versions 1.4 or 1.5. Chocolate Descent 2 will only work with Descent 2's 
commercial release (PC or Mac), version 1.2. 

License:
All the original code is under the terms of Parallax's Source License. 
New contributions are available under the terms of the MIT License, 
as described in COPYING.TXT.
Some contributions fall either under a 3-point BSD license, or
the D1X license, as described in COPYING.TXT
