# my_ldd

## Тесты

Ожидаемый output собирался руками через `lddtree` и `symtree` из пакета `pax-utils`.

Запуск тестов:
```shell
$ mkdir build && cd build
$ cmake .. && make all
$ ctest
```