i='11'
../pkgchecker test$i.bpkg -hashes_of 808d13d9ed9f9d7242ca5a95f1d879c734f88511360cf905b5fa6a4e4d0dbea4 > test$i.out 

#Surpress output for not
diff test$i.out test$i.expected > /dev/null

if [ $? -eq 0 ]; then
        echo "Output matches"
else
        echo "Outputs do NOT match!"
fi

