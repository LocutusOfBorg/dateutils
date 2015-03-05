#!/bin/sh

BEG="1917-01-01"
END="4095-01-01"
if test "${have_gdate_2039}" != "yes"; then
	BEG="1971-01-01"
	END="2038-01-01"
fi

if test "${have_gdate}" != "yes"; then
	## SKIP in new automake
	exit 77
fi

TOOLDIR="$(pwd)/../src"

DSEQ="${TOOLDIR}/dseq"

foo=`mktemp "/tmp/tmp.XXXXXXXXXX"`
bar=`mktemp "/tmp/tmp.XXXXXXXXXX"`

"${DSEQ}" "${BEG}" +1y "${END}" -f '%F	%a' > "${foo}"

## strip month and dom from BEG and END in a way that works on dash
BEG=`echo ${BEG} | sed 's/-.*//'`
END=`echo ${END} | sed 's/-.*//'`
for y in `seq ${BEG} ${END}`; do
	TZ=UTC LANG=C LC_ALL=C "${GDATE}" -d "${y}-01-01" '+%F	%a'
done > "${bar}"

diff "${foo}" "${bar}"
rc=${?}

rm -f "${foo}" "${bar}"

exit ${rc}
