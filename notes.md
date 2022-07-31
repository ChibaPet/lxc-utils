Run from the host system - general settings:

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

---

Run from the host system - container-specific:

~~~
# CONTAINERNAME=testcontainer
# CONTAINERMAC=DE:AD:BE:EF:00:00

# mkdir -p /var/lib/lxc/${CONTAINERNAME}
# cat > /var/lib/lxc/${CONTAINERNAME}/config <<END
lxc.uts.name = ${CONTAINERNAME}
lxc.rootfs.path = dir:/srv/lxc/${CONTAINERNAME}
lxc.net.0.type = veth
lxc.net.0.link = br0
lxc.net.0.flags = up
lxc.net.0.hwaddr = ${CONTAINERMAC}
lxc.idmap = u 0 100000 65536
lxc.idmap = g 0 100000 65536
lxc.include = /usr/share/lxc/config/debian.common.conf
END
~~~

Don't forget to add /etc/subuid and /etc/subgid ranges for root! Use that
range in the idmap config. Then _after everything's set up,_ but before you
launch, you can use the depriv tool to map the container tree to this
range:

~~~
# depriv /some/where/${CONTAINERNAME} 100000
~~~

---

Run this from the directory/dataset that contains the top level of your
container:

~~~
# MYDOMAIN="my.domain"

# http_proxy="http://cache.${MYDOMAIN}:3142" \
    debootstrap --no-merged-usr --arch=amd64 --variant=minbase \
    --include=sysvinit-core,libelogind0 \
    --exclude=systemd,systemd-sysv,libnss-systemd,libsystemd0 \
    bullseye ${CONTAINERNAME}/

# echo $CONTAINERNAME > ${CONTAINERNAME}/etc/hostname
# mkdir -p ${CONTAINERNAME}/etc/network
# echo "auto eth0" >> ${CONTAINERNAME}/etc/network/interfaces
# echo "iface eth0 inet dhcp" >> ${CONTAINERNAME}/etc/network/interfaces

# mkdir -p ${CONTAINERNAME}/etc/apt/preferences.d
# cat <<END > ${CONTAINERNAME}/etc/apt/preferences.d/no-systemd
Package: systemd
Pin: release *
Pin-Priority: -1

Package: systemd:i386
Pin: release *
Pin-Priority: -1

Package: systemd-sysv
Pin: release *
Pin-Priority: -1

Package: libnss-systemd
Pin: release *
Pin-Priority: -1
END

# mkdir -p ${CONTAINERNAME}/etc/apt/apt.conf.d
# cat > ${CONTAINERNAME}/etc/apt/apt.conf.d/02proxy <<END
Acquire::http::Proxy "http://cache.${MYDOMAIN}:3142";
END

# sed -i "/^[56]:23:respawn:\/sbin\/getty/ s/^/#/" ${CONTAINERNAME}/etc/inittab
# sed -i "s/^pf::powerwait:.*/pf::powerwait:\/etc\/init.d\/rc 0/g" \
    ${CONTAINERNAME}/etc/inittab
~~~

---

In a chroot, to finish out what debootstrap would have done if it were
possible to have it select an init system on its own:

~~~
# apt-cache dumpavail | perl -00 -ne '/^Package: (.*)/m && print "$1\n"
    if (/^Priority: (?:required|important|standard)/m
        and ! /^Package: .*systemd/m)' | xargs apt --yes install

# apt install --yes curl bsd-mailx locales man vim-nox net-tools \
    ifupdown bash-completion patch

# locale-gen en_US.UTF-8
# dpkg-reconfigure locales
# dpkg-reconfigure tzdata

# apt install --yes openssh-server rsync

# passwd root
# adduser --add_extra_groups myname
~~~

---

Now would probably be a reasonable time for:

~~~
# depriv ${CONTAINERNAME} 100000
~~~

...as long as you get the location and the UID base right.
