#!/bin/bash

rm *.o

echo compile muxerimpl.cpp
g++ -Wall -c muxerimpl.cpp
echo compile mkvmuxer.cpp
g++ -Wall -I../libebml -I../libmatroska -c mkvmuxer.cpp
echo compile mkvdemuxer.cpp
g++ -Wall -I../libebml -I../libmatroska -c mkvdemuxer.cpp

rm libmkvmuxer.a
rm -rf templib
mkdir templib
cd templib
echo archive libmkvmuxer.a
ar x ../../libebml/make/linux/libebml.a
ar x ../../libmatroska/make/linux/libmatroska.a
ar rcs ../libmkvmuxer.a *.o ../*.o
ranlib ../libmkvmuxer.a
cd ..
rm -rf templib

echo compile and link idmuxer
g++ -Wall -I../libebml -I../libmatroska -o idmuxer idmuxer.cpp ./libmkvmuxer.a
echo compile and link iddemuxer
g++ -Wall -o iddemuxer iddemuxer.cpp ./libmkvmuxer.a

echo complete
