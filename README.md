# captester
usb capacity tester (to detect fake-capacity USB drives' real capacity)
Warning: 
# Warning: 
- this is a ***DESTRUCTIVE*** scan, it will delete everything on your usb device while scanning.
- this program use north of 2GB RAM while scanning. (would be possible to use less, but it would also be slower)
# Usage: 
Start as root/administrator from cmd/terminal.
- ./captester
will list all the usb devices it can find (for example `/dev/sdX`)
- ./captester /dev/sdX
will scan usb device /dev/sdX.
# compiling
this seems to work: g++ -std=c++17 -Ofast -Wall -Wextra -Wpedantic -Werror -o captester captester.cpp

# pre-compiled binaries

Windows and Linux binaries are available at https://github.com/divinity76/captester/releases/download/v1.0.0-beta1/captester.zip
