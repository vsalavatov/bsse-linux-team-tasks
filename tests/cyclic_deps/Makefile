main: even.c odd.c main_even.c main_odd.c
	gcc -o even.o -fPIC -c even.c
	gcc -o odd.o -fPIC -c odd.c
	gcc -o libeven.so -shared -L"$(CURDIR)" even.o
	gcc -o libodd.so -shared -L"$(CURDIR)" -leven odd.o
	gcc -o libeven.so -shared -L"$(CURDIR)" -lodd even.o
	gcc -o cyclic_deps_elf -L"$(CURDIR)" -leven -Wl,-rpath="$(CURDIR)" main_even.c
	# gcc -o main_odd_elf -L"$(CURDIR)" -lodd -Wl,-rpath="$(CURDIR)" main_odd.c

clean:
	rm -f *.so *.o main_odd_elf main_even_elf cyclic_deps_elf

.PHONY: clean
