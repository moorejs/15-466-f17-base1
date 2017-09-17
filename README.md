# Escape the Courtyard

Escape the Courtyard is Jared Moore's implementation of [*Make and Escape*](http://graphics.cs.cmu.edu/courses/15-466-f17/game1-designs/hungyuc/) for game1 in 15-466-f17.

*Include a Screenshot Here*

## Asset Pipeline

For this game, I built an asset pipeline with a NodeJS script. It would listen for changes to files for files in multiple directories (given as command line arguments).

When a .xcf file changes, it runs a command to make a .png of that file. When a .info file changes, it writes a binary file (.file) containing a header with the size of the binary file and then the data. The data is expected to be 6 comma separated float values per line and is supposed to be the texture atlas mapping (minx, miny, maxx, maxy, centerx, centery). The center was never used. Some fluff was also allowed on each line but is parsed out. This was somewhat cumbersome and something I would like to change in the future. When a .cpp file changes, the code is compiled and the current process is kill and started anew. This didn't work so well so I didn't use it much.

The .file was processed using `std::ifstream.read` all at once.

## Architecture

Keypresses were tracked using SDL's `GetKeyboardState`. Each frame they were copied into another array `prevKeys` so that one could easily see detect the first frame someone pressed or let go of a key (e.g. `!prevKeys[...A] && keys[...A]`).

My code used `Circle`s for area's that player could interact with. `BoundedBox`es were useful for doing collisions. Both these structs had a `contains` method that was convenient.

`Object`s contain sprites, position, bounding box, and size.

A major of the code is dealing with `Item`s. Each of the three segments of the map have their own set of `Item`s. An item has a `Circle` for interaction (picking up), a field tracking what is added to the crafting inventory at the workbench if the items is brought there, and an `Object`. The player may hold one item at a time.

The crafting inventory was implemented with a enum bitfield which was not necessary but kind of cool.

## Reflection

One thing that was difficult was managing all of the items/sprites. It was a lot of work to make them interactable and placed in the right places around the map. That was made even more difficult when it was necessary that the player carried them and could go from one part of the map to another. If I had more time, I could have made a cleaner system for how items were handled in general.

I would automate the placement of bounding box collision areas somehow, maybe using an black and white image. Additionally, as I said before, I would make the pipeline more automated (maybe have something that can find the minx,miny etc. based on alpha in the images) because specifying the textures positions was cumbersome, especially before I added pixel support.

The design document was pretty clear, except for the fact that it left out where to place the items used for the crafting workbench. I decided to scatter these throughout the various segments of the map. Additionally, there was no walking animation provided, so it didn't turn out looking great. Doing the outline for interaction as specified by the design was not something I found feasible so I left that out.

# About Base1

This game is based on Base1, starter code for game1 in the 15-466-f17 course. It was developed by Jim McCann, and is released into the public domain.

## Requirements

 - modern C++ compiler
 - glm
 - libSDL2
 - libpng

On Linux or OSX these requirements should be available from your package manager without too much hassle.

## Building

This code has been set up to be built with [FT jam](https://www.freetype.org/jam/).

### Getting Jam

For more information on Jam, see the [Jam Documentation](https://www.perforce.com/documentation/jam-documentation) page at Perforce, which includes both reference documentation and a getting started guide.

On unixish OSs, Jam is available from your package manager:
```
	brew install ftjam #on OSX
	apt get ftjam #on Debian-ish Linux
```

On Windows, you can get a binary [from sourceforge](https://sourceforge.net/projects/freetype/files/ftjam/2.5.2/ftjam-2.5.2-win32.zip/download),
and put it somewhere in your `%PATH%`.
(Possibly: also set the `JAM_TOOLSET` variable to `VISUALC`.)

### Bulding
Open a terminal (on windows, a Visual Studio Command Prompt), change to this directory, and type:
```
	jam
```

### Building (local libs)

Depending on your OSX, clone 
[kit-libs-linux](https://github.com/ixchow/kit-libs-linux),
[kit-libs-osx](https://github.com/ixchow/kit-libs-osx),
or [kit-libs-win](https://github.com/ixchow/kit-libs-win)
as a subdirectory of the current directory.

The Jamfile sets up library and header search paths such that local libraries will be preferred over system libraries.
