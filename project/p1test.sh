#compile the program:

make pkgchecker
mv pkgchecker tests/part1
cd tests/part1

for i in $(seq 1 14); do
	echo $i
	if [ $i -lt 10 ]; then
		cd Test0$i
		echo "Executing test0$i.sh"
		if [ $i -eq 5 ]; then
			echo "deleting test05.data to ensure output matches"
			rm test05.data
		fi
		./test"0$i".sh
		cd ..
	else
		cd Test$i
		echo "Executing test$i.sh"
		./test$i.sh
		cd ..
	fi
done


