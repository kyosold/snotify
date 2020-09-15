# snotify
A queue service

# description
use sn-inq to add a file to queue, it automatically calls the specified program, 
it can be retry when specified program failure

# install
```bash
make
make install
```
it install to /var/snotify directory.

# in queue
```bash
/var/snotify/bin/sn-inq
```

# out queue
it automatically calls the specified program, the program write to rspawn how it process queue:

```
K: successful delivery and delete queue
D: failure delivery and delete queue
Z: failure delivery and next retry 
```

