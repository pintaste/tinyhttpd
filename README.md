# tinyhttpd
tinyhttpd is a very simple webserver wrote by J. David Blackstone. 

This repository forked from the [offical source](https://sourceforge.net/projects/tinyhttpd/) for learning purpose.


## Operationg System
- Ubuntu 22.10 kinetic

## Before compile the program
### install perl
using following command to install perl
```
which perl # check if perl is installed
sudo apt-get install perl # install perl
```

### install perl-cgi
using following command to install perl-cgi
```
perl -MCPAN -e shell
install CGI.pm
```

in terminal, type the following command to check the version of CGI.pm
```
perl -MCGI -e 'print "CGI.pm version $CGI::VERSION\n";' 
```
if the terminal shows CGI.pm version 4.xx, it means the installation is successful.

### permission modification
change the permission of the index.html under the /htdocs
```
sudo chmod 600 index.html
```

### modify the check.cgi and color.cgi
open both check.cgi and color.cgi /htdocs, change the line 1 to the following
```
#!/usr/bin/perl -Tw
```

## How to compile the program and run
- remove -lsocket from the Makefile.
- make
- ./tinyhttpd
- open the browser and type 
```
localhost:8888/ # 8888 is the port number displayed in the terminal
```
- enter the color to change the background color of the web page


### How to solve: Can't locate CGI.pm in @INC (you may need to install the CGI module)
using following command
```
sudo apt install libcgi-ajax-perl
sudo apt install libcgi-application-perl
```
Reference: https://blog.csdn.net/weixin_45808445/article/details/117161707
