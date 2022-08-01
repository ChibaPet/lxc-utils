# lxc-utils
Tools for composing content for containers.

***Warning - the following instructions will ravage your machine and then eat all the food in your refrigerator, and unlike the dwarves, they won't be kidding about breaking your dishes.***

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
solution was shifting the ownership of the directory tree to the correct
range, which I've addressed with the depriv program, found in this
repository.

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
Contact me if you want to see this happen sooner. For now I've included my
notes that demonstrate setting up a root-managed unprivileged container
using a host bridge, from scratch using debootstrap.

---

After writing this, I encountered
[shiftfs](https://discuss.linuxcontainers.org/t/trying-out-shiftfs/5155)
which does something similar, but as a kernel module. In the introduction,
they say "This allows for instant creation and startup of unprivileged
containers as no costly filesystem remapping is needed on creation or
startup."

Here's the cost of processing a complete Debian minimal install, sitting on
a ZFS mirror atop LUKS, all on 5400 RPM spinning rust:

~~~
/srv/lxc# time depriv /srv/lxc/base 100000

real	0m0.370s
user	0m0.020s
sys	0m0.350s
~~~

Yes, that's a bit over a third of a second.
