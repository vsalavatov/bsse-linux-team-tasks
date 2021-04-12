mkdir build
cd build
cmake .. && make all
mkdir tmp
./bablib tmp
python ../test.py
fusermount -u tmp
