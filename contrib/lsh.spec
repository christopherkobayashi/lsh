Summary:        lsh - secure connections
Name:           lsh
Version:        0.1.19
Release:        1
Copyright:      GPL
Group:          Application/Internet
Source0:       
ftp://ftp.lysator.liu.se/pub/security/lsh/%{name}-%{version}.tar.gz 
Source1:        lshd.rhlinux.init
BuildRoot:      /var/tmp/%{name}-%{version}-root
Prefix:         /usr
Packager:       Thayne Harbaugh <thayne@plug.org>
URL:            http://www.net.lut.ac.uk/psst/
Requires:       chkconfig


%description 
lsh impliments the secsh2 protocol


%prep
%setup


%build
CFLAGS="$RPM_OPT_FLAGS" ./configure --prefix=%prefix
if [ "$SMP" != "" ]; then
  (make "MAKE=gmake -k -j $SMP"; exit 0)
  gmake
else
  gmake
fi


%install
rm -rf $RPM_BUILD_ROOT

gmake prefix=$RPM_BUILD_ROOT%{prefix} install

( for man in doc/*.[0-9]
do
        MAN_NUM=`echo $man | sed 's/.*\.//'`
        install -d -m 0755 $RPM_BUILD_ROOT%{prefix}/man/man$MAN_NUM
        install -m 0644 $man $RPM_BUILD_ROOT%{prefix}/man/man$MAN_NUM
        rm -f $man
done )

install -d -m 0755 $RPM_BUILD_ROOT/etc/rc.d/init.d

install -m 0755 $RPM_SOURCE_DIR/lshd.rhlinux.init \
        $RPM_BUILD_ROOT/etc/rc.d/init.d/lshd

strip $RPM_BUILD_ROOT%{prefix}/bin/lsh
strip $RPM_BUILD_ROOT%{prefix}/bin/lsh_keygen
strip $RPM_BUILD_ROOT%{prefix}/bin/lsh_writekey
# strip $RPM_BUILD_ROOT%{prefix}/lib/*
strip $RPM_BUILD_ROOT%{prefix}/sbin/*

rm -rf doc/Makefile*


%clean
rm -rf $RPM_BUILD_ROOT


%post
chkconfig --add lshd
if [ ! -e /etc/lsh_host_key -o ! -e /etc/lsh_host_key.pub ]
then
        rm -f /etc/lsh_host_key*
        /usr/bin/lsh_keygen -l 8 | /usr/bin/lsh_writekey
/etc/lsh_host_key
fi


%preun
if [ "$1" -eq 1 ]
then
        chkconfig --del lshd || exit 0
fi


%files 
%defattr(-, root, root)

%doc AUTHORS COPYING ChangeLog FAQ NEWS README
%doc doc

%config/etc/rc.d/init.d/lshd
%{prefix}/bin/*
%{prefix}/man/*/*
%{prefix}/sbin/*


%changelog

* Thu Sep 28 1999 Thayne Harbaugh <thayne@northsky.com>
- first rpm

