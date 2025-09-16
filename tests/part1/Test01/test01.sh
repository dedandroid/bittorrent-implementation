../pkgchecker test01.bpkg -all_hashes > test01.out

#Surpress output for not
diff test01.out test01.expected > /dev/null

if [ $? -eq 0 ]; then
	echo "Output matches"
else
	echo "Outputs do NOT match!"
fi

