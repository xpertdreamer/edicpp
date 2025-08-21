#!/bin/bash
# build.sh — сборка TermEditor

# Проверка на gcc
if ! command -v gcc &> /dev/null
then
    echo "Ошибка: gcc не установлен. Пожалуйста, установите GCC."
    exit 1
fi

# Проверка на cmake
if ! command -v cmake &> /dev/null
then
    echo "Ошибка: cmake не установлен. Пожалуйста, установите CMake."
    exit 1
fi

# Создание папки сборки
mkdir -p build
cd build

# Генерация сборки через cmake
cmake ..
if [ $? -ne 0 ]; then
    echo "CMake configuration failed."
    exit 1
fi

# Компиляция
make
if [ $? -ne 0 ]; then
    echo "Сборка завершилась ошибкой."
    exit 1
fi

echo "Сборка завершена успешно!"
echo

# Предложение добавить в PATH
echo "Хотите добавить TermEditor в PATH для текущей сессии? (y/n)"
read -r addpath
if [[ "$addpath" == "y" || "$addpath" == "Y" ]]; then
    export PATH="$PWD:$PATH"
    echo "Добавлено $PWD в PATH. Теперь вы можете запускать 'termeditor' из любого места."
fi

echo "Готово!"
