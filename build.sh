gcc *.c -std=c11 -Wall -Wextra -Wpedantic -Wno-unused-parameter --static `sdl2-config --libs --static-libs` -o sleepdart
