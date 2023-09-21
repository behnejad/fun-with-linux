#!/bin/bash
YEAR=`date +"%Y"`
MONTH=`date +"%m"`
DAY=`date +"%e"`
DNAME=`date +"%a"`
MNAME=`date +"%b"`
gMonths=( 31 28 31 30 31 30 31 31 30 31 30 31 )
jMonths=( 31 31 31 31 31 31 30 30 30 30 30 29 )
jMonthNames=( null Farvardin Ordibehesht Khordad Tir Mordad Shahrivar Mehr Aban Azar Dey Bahman Esfand )
NAME=''
case $DNAME in
	'Sun')
		NAME="Yekshanbe"
		;;
	'Mon')
		NAME="Doshanbe"
		;;
	'Tue')
		NAME="Seshanbe"
		;;
	'Wed')
		NAME="Charshanbe"
		;;
	'Thu')
		NAME="Panjshanbe"
		;;
	'Fri')
		NAME="Jome"
		;;
	'Sat')
		NAME="Shanbe"
		;;
esac

#./jdates -f "%DD %d %MM %Y"

function georgian2jalali()
{
	geoY=$1
	geoM=$2
	geoD=$3

	geoY=$( echo "$geoY - 1600" | bc )
	geoM=$( echo "$geoM - 1" | bc )
	geoD=$( echo "$geoD - 1" | bc )

	geoDays=$( echo "365*$geoY+(($geoY+3)/4)-(($geoY+99)/100)+(($geoY+399)/400)" | bc )

	for(( i=0; i<$geoM; ++i ))
	do
		geoDays=$( echo "$geoDays + ${gMonths[$i]}" | bc )
	done
	# leap is missing
	if [ "$geoM" -gt 1 ]
	then
		if [ $( echo "$geoY % 4" | bc ) -eq 0 ]
		then
			if [ $( echo "$geoY%100" | bc ) -ne 0 ]
			then
				geoDays=$( echo "$geoDays+1" | bc )
			fi
		else
			if [ $( echo "$geoY % 400" | bc ) -eq 0 ]
			then
				geoDays=$( echo "$geoDays+1" | bc )
			fi
		fi
	fi

	geoDays=$( echo "$geoDays + $geoD" | bc )
	jllDays=$( echo "$geoDays - 79" | bc )
	jllNp=$( echo "$jllDays / 12053" | bc )
	jllDays=$( echo "$jllDays % 12053" | bc )
	jllY=$( echo "979+(33*$jllNp)+(4*($jllDays/1461))" | bc )
	jllDays=$( echo "$jllDays % 1461" | bc )

	if [ "$jllDays" -ge 366 ]
	then
		jllY=$( echo "$jllY+(($jllDays-1)/365)" | bc )
		jllDays=$( echo "($jllDays-1)%365" | bc )
	fi

	for(( i=0; (($i<11)&&($jllDays>=${jMonths[$i]})) ; ++i ))
	do
		jllDays=$( echo "$jllDays-${jMonths[$i]}" | bc )
	done
	
	
	echo `cat /etc/hostname` "|" $NAME `echo "$jllDays+1" | bc` ${jMonthNames[$jllNp]} $jllY "|" `date +"%X"`
}

georgian2jalali $YEAR $MONTH $DAY

