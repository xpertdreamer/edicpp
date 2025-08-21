#!/bin/bash
# build.sh — сборка edi с переносом конфигурации и добавлением в PATH

# Проверка на gcc
if ! command -v gcc &> /dev/null
then
    echo "Ошибка: gcc не установлен. Пожалуйста, установите GCC."
    exit 1
fi
echo "gcc найден!"

# Проверка на cmake
if ! command -v cmake &> /dev/null
then
    echo "Ошибка: cmake не установлен. Пожалуйста, установите CMake."
    exit 1
fi
echo "cmake найден!"

# Определение корневой папки проекта (где находится build.sh)
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Создание папки сборки
mkdir -p "$ROOT_DIR/build"
cd "$ROOT_DIR/build"
echo "папка build создана!"

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

# Предложение добавить в PATH для текущей сессии
echo
echo "Хотите добавить edi в PATH для текущей сессии и навсегда? (y/n)"
read -r addpath
if [[ "$addpath" == "y" || "$addpath" == "Y" ]]; then
    export PATH="$PWD:$PATH"
    echo "Добавлено $PWD в PATH для текущей сессии."

    # Определяем shell и файл конфигурации
    SHELL_NAME=$(basename "$SHELL")
    case "$SHELL_NAME" in
        bash)
            RC_FILE="$HOME/.bashrc"
            ;;
        zsh)
            RC_FILE="$HOME/.zshrc"
            ;;
        *)
            RC_FILE=""
            ;;
    esac

    if [[ -n "$RC_FILE" ]]; then
        # Проверка, есть ли уже запись в rc-файле
        if ! grep -Fxq "export PATH=\"$PWD:\$PATH\"" "$RC_FILE"; then
            echo "" >> "$RC_FILE"
            echo "# Добавление edi в PATH" >> "$RC_FILE"
            echo "export PATH=\"$PWD:\$PATH\"" >> "$RC_FILE"
            echo "Добавлено $PWD в PATH в $RC_FILE."
        else
            echo "PATH уже содержит $PWD в $RC_FILE."
        fi
        echo "Чтобы изменения вступили в силу, перезапустите терминал или выполните: source $RC_FILE"
    else
        echo "Не удалось определить shell, добавление в rc-файл не выполнено."
    fi
fi

echo "Готово!"
