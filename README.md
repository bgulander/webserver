webhttpd 2.0 - Yet Another Web Server
===========

[![Build Status](https://travis-ci.org/eamars/webserver.svg?branch=master)](https://travis-ci.org/eamars/webserver)

A simple HTTP server that provides minimal support of HTTP protocol.

Version 1.0

Features:
- HTTP_GET & HTTP_POST implementation
- Basic CGI execution
- Virtual hosts

Version 2.0

Features:
- Dynamic page generated by Python
- Static HTML pages are no longer supported


Installation
------------

Download and build from source:

    git clone git@github.com:eamars/webserver.git

webhttpd does not need any external dependencies.

Build webhttpd:

    make default

Build test

    make test

Usage
-----

You need to prepare the site package before starting webhttpd server. The site package folder requires site-config to provide basic information for starting a web server.

An example site-config is shown below

```dosini
# My site

# Server configuration
# Name of current running instance
server_name = My site

# Port to bind
server_port = 80

# dir
default_dir = site-package

# Default pages
default_index_page = /index.py
default_404_page = /404.py
default_favicon_image = /favicon.py
default_robots_text = /robots.py
```

To start the webhttpd server:

    ./src/webhttpd start site-package

To terminate the webhttp server:

    ./src/webhttpd stop site-package


Contributions
-------------

- joyent for [http-parser](https://github.com/joyent/http-parser)
