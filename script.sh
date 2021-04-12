mkdir build
cd build
cmake .. && make all
mkdir -p tmp
./bablib tmp
python ../test.py
fusermount -u tmp
