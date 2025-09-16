../pkgchecker test13.bpkg -all_hashes > test13.out

#Surpress output for not
diff test13.out test13.expected > /dev/null

if [ $? -eq 0 ]; then
	echo "Output matches"
else
	echo "Outputs do NOT match!"
fi

