#!/usr/bin/python3

import re

with open("http.dict", "w+") as o:
    with open("headers", "r") as f:
        o.write("# HTTP Request Headers\n")
        content = f.readlines()
        for line in content:
            print("{line} {header}".format(line=line.strip(), header=re.match("(.*):", line.strip()).group(1)))
            o.write("{header}=\"{line}\"\n".format(line=line.strip(), header=re.match("(.*):", line.strip()).group(1)))

    o.write("\n# HTTP Protocol Methods\n")
    with open("http-protocol-methods.txt", "r") as f:
        content = f.readlines()
        for line in content:
            print("{line}=\"{line}\"".format(line=line.strip()))
            o.write("{line}=\"{line}\"\n".format(line=line.strip()))

    o.write("\n# HTTP URI Types\n")
    with open("known-uri-types.txt", "r") as f:
        content = f.readlines()
        for line in content:
            print("{uri}=\"{line}//\"".format(line=line.strip(), uri=re.match("(.*):", line.strip()).group(1)))
            o.write("{uri}=\"{line}//\"\n".format(line=line.strip(), uri=re.match("(.*):", line.strip()).group(1)))

    o.write("\n# HTTP URI Paths\n")
    o.write("localhost=\"localhost/\"\n")
    o.write("base=\"/\"\n")
