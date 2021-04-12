mkdir build
cd build
cmake .. && make all
mkdir -p tmp
./bablib tmp
python3 ../test.py
fusermount -u tmp
