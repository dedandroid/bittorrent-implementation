for i in $(seq 1 12);
do 
	if [ $i -lt 10 ]; then
		cp $1 Test0$i
	else
		#Formatting of the last file is a bit different.
		cp $1 Test$i
	fi

done

