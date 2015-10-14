make clean; make -j4 tritontalk;
(echo "exit") | ./tritontalk -r 1 -s 1
make clean;
