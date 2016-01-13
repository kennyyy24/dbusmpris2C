all:
	gcc -Wall -o Example example.c `pkg-config --cflags dbus-1` `pkg-config --libs dbus-1`

clean:
	rm Example