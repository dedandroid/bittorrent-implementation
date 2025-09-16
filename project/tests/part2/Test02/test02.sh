#!/bin/bash
i='2'

cd ..
./peer1 Test0$i/test0$i.in > Test0$i/test0$i.out
cd Test0$i
diff test0$i.out test0$i.expected
if [ $? -ne 0 ]; then
	echo "Outputs don't match!"
else
	echo "Outputs match!"
fi
