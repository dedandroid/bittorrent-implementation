i='10'
../pkgchecker test$i.bpkg -hashes_of 1db14bf710c0cc9837ed06c48dc7e83655b03dd8742aa289e610ab198146b273 > test$i.out

#Surpress output for not
diff test$i.out test$i.expected > /dev/null

if [ $? -eq 0 ]; then
        echo "Output matches"
else
        echo "Outputs do NOT match!"
fi
