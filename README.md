# lxc-utils
Tools for composing content for containers.

***Warning - the following instructions will ravage your machine and then eat all the food in your refrigerator, and unlike the dwarves, they won't be kidding about breaking your dishes.***

_Buggy. Do not use unless you're feeling adventurous and like supporting
your own systems._

---

In exploring LXC as a stand-in for FreeBSD jails, the notion of
unprivileged containers run by root arose. However, according to the
[documentation](https://linuxcontainers.org/lxc/getting-started/):

> most distribution templates simply won't work with those. Instead you
> should use the "download" template which will provide you with pre-built
> images of the distributions that are known to work in such an environment.

Rather than selling a fish, it might be better to teach a person to fish.
You can build a sysvinit-based, unprivileged Debian container without a
great deal of pain. One challenge that didn't present an immediate
solution, however, was shifting the ownership of the directory tree to the
correct range.

_NOTE: As of 2022-07-31 the author (me, Mason Loring Bliss) is still
exploring LXC, and there might be concepts of which I'm unaware that
obviate all of this._

_UNRELATED NOTE while I'm at it: GitHub doesn't have an option for
four-clause BSD licenses, and that's what this is. Fedora doesn't like CC0
any more, so we'll use an old favourite instead._

This _depriv_ tool can be invoked generally as follows:

~~~
# depriv /srv/lxc/testcontainer 100000 100000
~~~

...where the first argument is the path to a container, the second is the
start of the subuid range to be used for the unprivileged container, and
the third is the start of the subgid range. If a subgid base is not
specified, the subuid base will be used for GID. Each file in the tree will
have the base UID and GID modifiers added to UID and GID, after which the
original file mode will be re-applied for anything that's not a symlink.
(POSIX dictates that the system chown and chgrp strip special mode bits,
which ends up not being what we want for the files in the unprivileged
container.)

In general you'll want to run this as root, so ***be careful*** as you can
do great damage. The depriv tool attempts to do some sanity checking before
it operates on your filesystem, but only a little. If you do get yourself
into trouble, note that if the tool is invoked as 'repriv' it will subtract
the specified UID and GID values from things living in the specified tree.

At this writing, I intend to expand my (as yet unpublished) ZFS-aware VM
management tools to encompass LXC containers. This will mean that container
management will benefit from things the VM tools do today, like container
datasets including critical metadata as custom properties in the their ZFS
datasets, which the tools use for seamless migration between systems.
Contact me if you want to see this happen sooner. For now I'll include some
raw notes that follow my exploration in setting up root-managed
unprivileged containers.
