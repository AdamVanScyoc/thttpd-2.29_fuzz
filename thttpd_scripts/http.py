#!/usr/bin/python3

http_verbs = ["GET","HEAD","POST","PUT","DELETE","TRACE","OPTIONS","CONNECT","PATCH"]

for verb in http_verbs:
    for maj in range(0,4):
        for minor in range(0,4):
            with open("{verb}_HTTP_{maj}.{minor}".format(verb=verb,maj=maj,minor=minor), 'a+') as f:
                f.write("{verb} http://localhost/ HTTP/{maj}.{minor}\x0d\x0a\x0a".format(verb=verb,maj=maj,minor=minor))
