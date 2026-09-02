#!/bin/sh -e
# Copyright (c) 2026 Roger Brown.
# Licensed under the MIT License.

cleanup()
{
	rm -rf data rpms rpm.spec meta
}

umask 022

cleanup

trap cleanup 0

VERSION=$1

if test -z "$MAINTAINER"
then
	MAINTAINER="$(git config user.email)"
fi

test -n "$VERSION"
test -n "$MAINTAINER"

PWD=$(pwd)
ISRPM=false
ISDEB=false
ISFREEBSD=false

if test -f /etc/os-release
then
	for d in $( . /etc/os-release ; echo $ID $ID_LIKE )
	do
		case "$d" in
			debian )
				ISDEB=true
				;;
			fedora | centos | rhel | mariner | suse | opensuse )
				ISRPM=true
				;;
			freebsd )
				ISFREEBSD=true
				;;
		esac
	done
fi

SHORTNAME=$( . /etc/os-release ; echo $ID )
SHORTVERS=$( . /etc/os-release ; echo $VERSION_ID | sed y/\./\ / | while read A B; do echo $A; done )

case "$SHORTNAME" in
	debian )
		SHORTNAME=deb
		;;
	raspbian )
		SHORTNAME=rpi
		;;
	rhel )
		SHORTNAME=rh
		;;
	fedora )
		SHORTNAME=fc
		;;
	freebsd )
		SHORTNAME=fb
		;;
	ubuntu )
		SHORTVERS=$( . /etc/os-release ; echo $VERSION_ID | sed s/\\./\/g )
		;;
	* )
		;;
esac

IDVERSIONID="$SHORTNAME$SHORTVERS"

PKGNAME=mpwshell-motif

mkdir -p data/usr/bin rpms

cp mpwshell data/usr/bin

strip data/usr/bin/mpwshell

if $ISDEB
then
	DPKGARCH=$(dpkg --print-architecture)
	SIZE=$(du -sk data/usr | while read A B; do echo $A; done)
	mkdir data/DEBIAN
	DEPENDS=powershell
	for d in $(ldd mpwshell | grep libX | while read A B C D
		do
			case "$A" in
				libXm.so* | libXpm.* )
					dpkg -S $(realpath $C) | sed y/:/\ / | while read E F; do echo $E; done
					;;
				* )
					;;
			esac
		done
	)
	do
		DEPENDS="$DEPENDS, $d"
	done
	cat > data/DEBIAN/control <<EOF
Package: $PKGNAME
Version: $VERSION-$IDVERSIONID
Architecture: $DPKGARCH
Installed-Size: $SIZE
Depends: $DEPENDS
Maintainer: $MAINTAINER
Section: editors
Priority: extra
Description: Text Editor based on CDE dtpad with integrated PowerShell
 Text editor based on CDE dtpad integrates PowerShell directly into editor content
 .
EOF

	(
		set -e
		cd data
		tar --owner=0 --group=0 --create --gzip --file control.tar.gz -C DEBIAN control
		tar --owner=0 --group=0 --create --gzip --file data.tar.gz usr/bin/mpwshell
		echo 2.0 > debian-binary
		ar r "$PKGNAME"_"$VERSION-$IDVERSIONID"_"$DPKGARCH".deb debian-binary control.tar.gz data.tar.gz
		rm -rf DEBIAN control.tar.gz data.tar.gz debian-binary
	)

	mv data/"$PKGNAME"_"$VERSION-$IDVERSIONID"_"$DPKGARCH".deb .
fi

if $ISRPM
then
	(
		set -e

		cd "$PWD/data"

		cat <<EOF
%global source_date_epoch_from_changelog 0
Summary: Text Editor based on CDE dtpad with integrated PowerShell
Name: $PKGNAME
Version: $VERSION
Release: 1.$IDVERSIONID
Requires: powershell
Group: Applications/System
License: MIT
Prefix: /usr/bin

%description
Text editor based on CDE dtpad integrates PowerShell directly into editor content.

%files
%defattr(-,root,root)
%attr(555,root,root) /usr/bin/mpwshell

%clean
EOF
	) > rpm.spec

	rpmbuild --buildroot "$PWD/data" --define "_rpmdir $PWD/rpms" --define "_build_id_links none" -bb "$PWD/rpm.spec" 

	find rpms -name "*.rpm" -type f | while read N
	do
		mv "$N" .
	done
fi

if $ISFREEBSD
then
	(
		PREFIX=/usr/local
		mkdir meta
		
		(
			cat << EOF
name $PKGNAME
version $VERSION
desc "Text editor based on CDE dtpad integrates PowerShell directly into editor content."
www https://github.com/rhubarb-geek-nz/MPWShell
origin editors/mpwshell
comment Text Editor based on CDE dtpad with integrated PowerShell
maintainer $MAINTAINER
prefix $PREFIX
licenses: [
	"MIT"
]
EOF
			echo "deps: {"
			COMMA=false
			for d in powershell open-motif libXpm
			do
				ORIGIN=$(pkg query "%o" $d)
				VERS=$(pkg query "%v" $d)
				if $COMMA
				then
					echo ","
				fi
				echo -n "	$d: {origin: $ORIGIN, version: $VERS}"
				COMMA=true
			done
			echo
			echo "}"
		) > meta/MANIFEST

		mkdir data/usr/local
		mv data/usr/bin data/usr/local/bin
		echo bin/mpwshell > meta/PLIST

		pkg create -M meta/MANIFEST -o . -r data -v -p meta/PLIST
	)
fi
