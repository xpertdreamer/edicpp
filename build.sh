#!/bin/bash
# build.sh — сборка edi с переносом конфигурации

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

# Определение корневой папки проекта (где находится build.sh)
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Создание папки сборки
mkdir -p "$ROOT_DIR/build"
cd "$ROOT_DIR/build"

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

# Определяем OS для выбора папки конфигурации
if [[ "$OSTYPE" == "darwin"* ]]; then
    # macOS
    CONFIG_DIR="$HOME/Library/Application Support/edi"
else
    # Linux и прочие Unix
    CONFIG_DIR="$HOME/.config/edi"
fi

mkdir -p "$CONFIG_DIR"

# Перенос/копирование конфигурационного файла
CONFIG_FILE="$CONFIG_DIR/config.conf"
if [ -f "$CONFIG_FILE" ]; then
    echo "Конфигурационный файл уже существует в $CONFIG_DIR"
else
    if [ -f "$ROOT_DIR/config.conf" ]; then
        cp "$ROOT_DIR/config.conf" "$CONFIG_FILE"
        echo "Конфигурационный файл создан из шаблона в $CONFIG_DIR"
    else
        touch "$CONFIG_FILE"
        echo "Создан пустой конфигурационный файл: $CONFIG_FILE"
    fi
fi

# Предложение добавить в PATH
echo
echo "Хотите добавить edi в PATH для текущей сессии? (y/n)"
read -r addpath
if [[ "$addpath" == "y" || "$addpath" == "Y" ]]; then
    export PATH="$PWD:$PATH"
    echo "Добавлено $PWD в PATH. Теперь вы можете запускать 'edi' из любого места."
fi

echo "Готово!"
