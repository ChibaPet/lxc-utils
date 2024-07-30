Overview:

We're going to setup `/srv/lxc` with one directory (or ideally ZFS dataset)
per container. Each directory will be owned by root:root, and will have a
config file owned by `root:root`, and a rootfs owned by the bottom
subuid/subgid (root account) for the container.

The notion is that we can migrate containers around by sending the
appropriate dataset per container. Creating new containers might involve a
tool (forthcoming) to grab a kernel advisory lock on `/etc/sub{u,g}id` to
identify and provision a new range for the container. It's not yet clear to
me what the most sensible tool will be to migrate sub{u,g}id ranges - the
clearest solution that occurs to me right now (20230101) is a preparatory
check with the provisioning tool that will succeed or fail based on its
ability to provision the desired range on the remote machine. If it can,
then the set can be transferred much the same way I do it today with my
(unpublished) VM tools. (Config will need to be renamed to keep it from
autostarting. Prior to move maybe, so we don't end up with the process
breaking leaving us with two containers that want to run?)

Wish list: an extended `/etc/sub{u,g}id` with support for comments, or (and I
know this could be racey) something like `/etc/sub{u,g}id.d`.

---

Run from the host system - shared settings for containers:

~~~
# apt install lxc libpam-cgfs uidmap
# cat > /etc/lxc/default.conf <<END
lxc.net.0.type = veth
lxc.net.0.link = br0
lxc.net.0.flags = up
lxc.apparmor.profile = generated
lxc.apparmor.allow_nesting = 1
lxc.arch = amd64
END
~~~

Run from the host system - path to configs and storage:

~~~
# cat > /etc/lxc/lxc.conf <<END
lxc.lxcpath = /srv/lxc
END
~~~

I don't know what mounts this automatically on some systems, but if you get
errors about not being able to allocate memory, you might verify that
`/sys/fs/cgroup` is mounted. If it's not, you can add this to `/etc/fstab`:

~~~
cgroup2 /sys/fs/cgroup cgroup2 rw,nosuid,nodev,noexec,relatime 0 0
~~~

Run from the host system - container-specific. Pick an appropriate subuid
range and assign it to root. (You can do one big range, but I like breaking
them up into 65,536-UID sets.)

~~~
# CONTAINERNAME=testcontainer
# CONTAINERMAC=DE:AD:BE:EF:00:00
# CONTAINERSUBUID=100000

# mkdir -p /srv/lxc/${CONTAINERNAME}
# cat > /srv/lxc/${CONTAINERNAME}/config <<END
lxc.uts.name = ${CONTAINERNAME}
lxc.rootfs.path = dir:/srv/lxc/${CONTAINERNAME}/rootfs
lxc.net.0.type = veth
lxc.net.0.link = br0
lxc.net.0.flags = up
lxc.net.0.hwaddr = ${CONTAINERMAC}
lxc.idmap = u 0 ${CONTAINERSUBUID} 65536
lxc.idmap = g 0 ${CONTAINERSUBUID} 65536
lxc.include = /usr/share/lxc/config/debian.common.conf
END
~~~

Don't forget to add `/etc/subuid` and `/etc/subgid` ranges for root! Use
that range in the idmap config. Then _after everything's set up,_ but
before you launch, you can use the depriv tool to map the container tree to
this range.

---

Run this from the directory/dataset that contains the top level of your
container - `/srv/lxc` in my examples. ~~Note that installing Bookworm from
Bullseye will force in usrmerge even if you've specified otherwise, pinned
it away, performed a blood sacrifice, etc. I'll update this for Bookworm
soon, but until then, realize that you'll probably want to upgrade your
host before trying to install new Bookworm guests. (This shouldn't be a
problem upgrading from within, and you could always do the container
debootstrap on a Bookworm system and migrate it to your existing Bullseye
host if you're not ready to upgrade it.)~~ (And, you could just use UsrMerge,
if you don't mind it breaking dpkg for [no compelling reason](https://wiki.debian.org/Teams/Dpkg/MergedUsr).)

Note that I serve init-system-helpers 1.64 from a local repository because
of UsrMerge breakage. You might wish to do the same, or, perhaps slicker,
you could pin the specific version. Once Bookworm's successor is released
the UsrMerge breakage will be less, and it'll be less painful allowing it
in, in which case the special handling will go away here.

Update: More recently installing from Bullseye won't wedge in UsrMerge, but
you'll see:

~~~
W: Upgrading non-merged-/usr environments post-bookworm is unsupported.
Only do this for CI/QA infrastructure that will be re-bootstrapped rather
than upgraded.
~~~

~~~
# cd /srv/lxc
# mkdir -p ${CONTAINERNAME}/rootfs

# MYDOMAIN="my.domain"

# http_proxy="http://cache.${MYDOMAIN}:3142" \
    debootstrap --no-merged-usr --arch=amd64 --variant=minbase \
    --include=sysvinit-core,libelogind0 \
    --exclude=systemd,systemd-sysv,libnss-systemd,libsystemd0 \
    bookworm ${CONTAINERNAME}/rootfs

# echo $CONTAINERNAME > ${CONTAINERNAME}/rootfs/etc/hostname
# mkdir -p ${CONTAINERNAME}/rootfs/etc/network
# echo "auto eth0" >> ${CONTAINERNAME}/rootfs/etc/network/interfaces
# echo "iface eth0 inet dhcp" >> ${CONTAINERNAME}/rootfs/etc/network/interfaces

# mkdir -p ${CONTAINERNAME}/rootfs/etc/apt/preferences.d
# cat <<END > ${CONTAINERNAME}/rootfs/etc/apt/preferences.d/no-systemd
Package:
    avahi-daemon:any
    src:usrmerge:any
    systemd:any
    systemd-sysv:any
    libnss-systemd:any
Pin: release *
Pin-Priority: -1

Package:
    init-system-helpers:any
Pin: origin repo.${MYDOMAIN}
Pin-Priority: 1000
END

# mkdir -p ${CONTAINERNAME}/rootfs/etc/apt/apt.conf.d
# cat > ${CONTAINERNAME}/rootfs/etc/apt/apt.conf.d/02proxy <<END
Acquire::http::Proxy "http://cache.${MYDOMAIN}:3142";
END

# sed -i "/^[56]:23:respawn:\/sbin\/getty/ s/^/#/" \
    ${CONTAINERNAME}/rootfs/etc/inittab
# sed -i "s/^pf::powerwait:.*/pf::powerwait:\/etc\/init.d\/rc 0/g" \
    ${CONTAINERNAME}/rootfs/etc/inittab
~~~

---

In a chroot, to finish out what `debootstrap` would have done if it were
possible to have it select an init system on its own:

~~~
# chroot ${CONTAINERNAME}/rootfs

# apt-cache dumpavail | perl -00 -ne '/^Package: (.*)/m && print "$1\n"
    if (/^Priority: (?:required|important|standard)/m
        and ! /^Package: .*systemd/m)' \
            | xargs apt --yes --allow-downgrades install

# apt install --yes curl bsd-mailx locales man vim-nox net-tools \
    ifupdown bash-completion patch openssh-server rsync \
    orphan-sysvinit-scripts rsyslog

# dpkg-reconfigure locales
# dpkg-reconfigure tzdata

# passwd root

# adduser --add_extra_groups myname

# exit
~~~

---

Now would probably be a reasonable time for:

~~~
# depriv ${CONTAINERNAME}/rootfs ${CONTAINERSUBUID}
~~~

...after you double-check the subuid and subgid ranges.

---

Migrating VMs to containers is relatively straightforward. Create a layout
in /srv/lxc as above, mount up the VM's disks, and rsync everything over.
Make the noted changes to /etc/inittab in the new guest. Make sure to empty
out the fstab as there will be no more local disks! Before you depriv, it's
safe enough to chroot in and remove kernel and initramfs packages,
bootloaders, and potentially other stuff.
