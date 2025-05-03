# CPU Hotplug Governor

A simple GTK+ GUI-based utility to manage CPU hotplugging on Linux systems. This tool allows users to enable or disable CPU cores dynamically, potentially improving energy efficiency or performance depending on system load.

## Features

- GTK+ 3 GUI interface  
- Control CPU online/offline state  
- Lightweight and responsive design  
- Compatible with most Linux distributions  

## Requirements

- **GCC**  
- **GTK+ 3 development libraries**  
- **make**  

Install the required dependencies (for Debian-based systems):

```bash
sudo apt-get install build-essential libgtk-3-dev
To build the project, simply run:

make
sudo ./cpu-hotplug-governor


Cleaning Up
make clean
