# captester
usb capacity tester (to detect fake-capacity USB drives)
Warning: this is a ***DESTRUCTIVE*** scan, it will delete everything on your usb device while scanning.
Warning: this program use north of 2GB RAM while scanning. (would be possible to use less, but it would also be slower)
# Usage: 
./captester /dev/sdX

# compiling
this seems to work: g++ -std=c++17 -Ofast -Wall -Wextra -Wpedantic -Werror -o captester captester.cpp

# pre-compiled binaries

todo