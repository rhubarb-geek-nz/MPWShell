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

MPWSHELL_DESKTOP=nz.geek.rhubarb.mpwshell.desktop

ICONPATH=$(grep ^Icon=/usr/ mpwshell.desktop | sed s/^Icon=//)
ICONDIR=$(dirname "$ICONPATH")

test -n "$ICONPATH"

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

test -f mpwshell
test -x mpwshell

if objdump -p mpwshell | grep NEEDED | grep libQt5Core > /dev/null
then
	PKGNAME=mpwshell-qt5
else
	if objdump -p mpwshell | grep NEEDED | grep libQt6Core > /dev/null
	then
		PKGNAME=mpwshell-qt6
	else
		PKGNAME=mpwshell-qt
	fi
fi

mkdir -p data/usr/bin rpms "data$ICONDIR" data/usr/share/applications

cp mpwshell data/usr/bin
cp mpwshell.png "data$ICONPATH"
cp mpwshell.desktop "data/usr/share/applications/$MPWSHELL_DESKTOP"

strip data/usr/bin/mpwshell

if $ISDEB
then
	DPKGARCH=$(dpkg --print-architecture)
	SIZE=$(du -sk data/usr | while read A B; do echo $A; done)
	mkdir data/DEBIAN
	DEPENDS=powershell
	for e in $(objdump -p mpwshell | grep NEEDED | while read A B; do echo $B; done)
	do
		case "$e" in
			libQt* )
				for d in $(ldd mpwshell | grep "$e" | while read A B C D
					do
						case "$A" in
							"$e" )
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
				;;
			* )
				;;
		esac
	done
	cat > data/DEBIAN/control <<EOF
Package: $PKGNAME
Version: $VERSION-$IDVERSIONID
Architecture: $DPKGARCH
Installed-Size: $SIZE
Depends: $DEPENDS
Maintainer: $MAINTAINER
Section: editors
Provides: mpwshell
Priority: extra
Description: Text Editor with integrated PowerShell
 Text editor implemented with Qt integrates PowerShell directly into editor content
 .
EOF

	(
		set -e
		cd data
		tar --owner=0 --group=0 --create --gzip --file control.tar.gz -C DEBIAN control
		tar --owner=0 --group=0 --create --gzip --file data.tar.gz $(find usr -type f)
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
Summary: Text Editor with integrated PowerShell
Name: $PKGNAME
Version: $VERSION
Release: 1.$IDVERSIONID
Requires: powershell
Provides: mpwshell
Group: Applications/System
License: MIT
Prefix: /usr

%description
Text editor implemented with Qt integrates PowerShell directly into editor content.

%files
%defattr(-,root,root)
%attr(555,root,root) /usr/bin/mpwshell
%attr(444,root,root) /usr/share/applications/$MPWSHELL_DESKTOP
%attr(444,root,root) $ICONPATH

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

		DEPENDS=powershell

		for a in $(objdump -p mpwshell | grep NEEDED | while read A B; do echo "$B"; done)
		do
			for b in $(ldd mpwshell | grep "$a" | while read A B C D; do echo "$C"; done)
			do
				if pkg which "$b" > /dev/null
				then
					NAME=
					for n in $(pkg which $b)
					do
						NAME="$n"
					done
					NAME=$(pkg info $NAME | grep "^Name" | sed y/:/\ / | while read A B; do echo $B; done )
					for c in $DEPENDS
					do
						if test "$c" = "$NAME"
						then
							NAME=
							break
						fi
					done
					if test -n "$NAME"
					then
						DEPENDS="$DEPENDS $NAME"
					fi
				fi
			done
		done
		
		(
			cat << EOF
name $PKGNAME
version $VERSION
desc "Text editor implemented in Qt integrates PowerShell directly into editor content."
www https://github.com/rhubarb-geek-nz/MPWShell
origin editors/mpwshell
comment Text Editor implemented in Qt with integrated PowerShell
maintainer $MAINTAINER
prefix $PREFIX
licenses: [
	"MIT"
]
EOF
			echo "deps: {"
			COMMA=false
			for d in $DEPENDS
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
		mv data/usr/share data/usr/local/share

		(
			cd "data$PREFIX"
			find * -type f
		) > meta/PLIST

		sed -i '' "s|^Icon=/usr/share/|Icon=/usr/local/share/|g" "data/usr/local/share/applications/$MPWSHELL_DESKTOP"

		pkg create -M meta/MANIFEST -o . -r data -v -p meta/PLIST
	)
fi
