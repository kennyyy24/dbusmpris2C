# dbusmpris2C
Intended to be a minimalist C, header only, DBus controller for video players using the MPRIS2 convention (vlc, omxplayer...).

Code is using the low-level libdbus library (no X server required).

## Compilation
Compile using the provided Makefile or with:

gcc -Wall -o Example example.c `pkg-config --cflags dbus-1` `pkg-config --libs dbus-1`

## Supported players
So far only two players have a (very) partial support:
- VLC
- omxplayer

Note that some functionnalities are only available for one or the other (since omxplayer doesn't fully respect MPRIS2 standard). See source code for details.

## Extend
To extend to another player, simply use the vlc model (which respects the MPRIS2 standard).

## References
MPRIS2 specifications can be found here: http://specifications.freedesktop.org/mpris-spec/latest/ .

omxplayer repository on GitHub: https://github.com/popcornmix/omxplayer .

