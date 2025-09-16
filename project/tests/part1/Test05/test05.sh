i='5'
../pkgchecker test0$i.bpkg -file_check > test0$i.out

#Surpress output for not
diff test0$i.out test0$i.expected > /dev/null

if [ $? -eq 0 ]; then
        echo "Output matches"
else
        echo "Outputs do NOT match!"
fi
