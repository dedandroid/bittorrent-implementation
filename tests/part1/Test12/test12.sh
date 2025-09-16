i='12'
../pkgchecker test$i.bpkg -hashes_of hehehehehehehahahahahah > test$i.out

#Surpress output for not
diff test$i.out test$i.expected > /dev/null

if [ $? -eq 0 ]; then
        echo "Output matches"
else
        echo "Outputs do NOT match!"
fi
