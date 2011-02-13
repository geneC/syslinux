#!/bin/sh

#######################################################
#  Round 1: get all the loacl and external symbols
#######################################################

for i in com32/elflink/modules/*.c32 core/isolinux.elf core/pxelinux.elf com32/elflink/ldlinux/*.c32
do
	# module=$(echo $i | sed "s/^\(.*\).o$/\1/")
	
	# remove the path infomation
	module=$(echo $i | sed "s/^.*\/\(.*\)$/\1/")

	readelf -s $i > temp.txt
	#Get the last 2 items of each line
	cut -c47- temp.txt > $module.txt 
	rm temp.txt

	#Get the unresolved symbols
	sed -n -e "/UND $/d" -e "/_GLOBAL_OFFSET_TABLE_/d" -e  "s/^.UND.\(.*\)$/\1/p" $module.txt > $module.ext

	#Get the local symbols
	sed -n -e "/UND/d" -e "/ABS/d" -e "/...[0-9] $/d" -e "/...[0-9] \./d" -e "/...[0-9]/p" $module.txt > $module.int 
	sed -i -e "s/^.....//g" $module.int
	sed -i -e "s/^\(.*\)$/\1 <$module>/g" $module.int

	cat $module.int >> all.txt
done


touch modules.dep

#######################################################
#  Round 2: get all the loacl and external symbols
#######################################################

# Consolidate the dependent modules to one line and remove
# the redundant ones, and the "core" 
rm_cr ()
{
	touch rmcr.tmp
	all_dep=$module:
	space=' '

	while read line
	do
		# skip the module which is alreay on the list 
		grep $line rmcr.tmp > /dev/null && continue

		# grep extlinux/isolinux and remove them
		echo $line | grep extlinux > /dev/null && continue
		echo $line | grep isolinux > /dev/null && continue
		echo $line | grep pxelinux > /dev/null && continue

		all_dep=$all_dep$space$line
		echo $all_dep > rmcr.tmp
	done

	echo $all_dep >> modules.dep
	rm rmcr.tmp
}

# Find the symbol belongs to which module by screening all.txt, do it
# one by one, and the result "resolve.tmp" will be a file like:
#	a.c32
#	b.c32
#	c.c32
resolve_sym ()
{
	touch resolve.tmp

	while read symbol 
	do
		# If no one provides the symbol we're trying to
		# resolve then add it to the list of unresolved
		# symbols.
		grep -q $symbol all.txt
		if [ $? -ne 0 ]; then
			# We only need to add the symbol once
			if [[ ! "$unresolved_symbols" =~ "$symbol" ]]; then
				unresolved_symbols="$unresolved_symbols $symbol"
			fi
		else
			#echo $symbol
			sed -n -e "s/^$symbol <\(.*\)>/\1/p" all.txt >> resolve.tmp
			#grep $symbol all.txt
		fi
	done

	rm_cr < resolve.tmp
	rm resolve.tmp
}

#only test name start with a/b
#rm [c-z]*.ext

if [ -e modules.dep ]
then
	rm modules.dep
	touch modules.dep
fi

# Don't need to resolve the core symbols
for i in extlinux isolinux pxelinux
do
	if [ -e $i.elf.ext ]
	then
		rm $i.elf.ext
	fi
done

for i in *.ext 
do
	module=$(echo $i | sed "s/^\(.*\).ext$/\1/")
	resolve_sym < $i
done

# Do some cleanup
rm *.txt
rm *.ext
rm *.int

if [ "$unresolved_symbols" ]; then
	echo "WARNING: These symbols could not be resolved:" $unresolved_symbols
fi

echo ELF modules dependency is bult up, pls check modules.dep!
