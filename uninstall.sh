#!/bin/bash
# uninstall.sh — полное удаление edi и его конфигурации

echo "Начинаем удаление edi..."

# Функция поиска и удаления бинарника
remove_executable() {
    local exe_name="edi"
    local found=0

    # Проверяем текущую папку build
    if [ -f "$PWD/build/$exe_name" ]; then
        rm "$PWD/build/$exe_name"
        echo "Удалён бинарник: $PWD/build/$exe_name"
        found=1
    fi

    # Проверяем PATH
    IFS=':' read -ra paths <<< "$PATH"
    for p in "${paths[@]}"; do
        if [ -f "$p/$exe_name" ]; then
            rm "$p/$exe_name"
            echo "Удалён бинарник из PATH: $p/$exe_name"
            found=1
        fi
    done

    if [ $found -eq 0 ]; then
        echo "Бинарник edi не найден."
    fi
}

# Функция удаления конфигурации
remove_config() {
    local config_dir
    if [[ "$OSTYPE" == "darwin"* ]]; then
        config_dir="$HOME/Library/Application Support/edi"
    else
        config_dir="$HOME/.config/edi"
    fi

    if [ -d "$config_dir" ]; then
        rm -rf "$config_dir"
        echo "Удалена конфигурация: $config_dir"
    else
        echo "Конфигурация не найдена: $config_dir"
    fi
}

# Функция очистки PATH из rc-файлов
cleanup_rc() {
    local rc_file=""
    local shell_name
    shell_name=$(basename "$SHELL")

    case "$shell_name" in
        bash)
            rc_file="$HOME/.bashrc"
            ;;
        zsh)
            rc_file="$HOME/.zshrc"
            ;;
    esac

    if [[ -n "$rc_file" && -f "$rc_file" ]]; then
        # Удаляем строки, добавленные скриптом build.sh
        sed -i.bak '/# Добавление edi в PATH/,+1d' "$rc_file"
        echo "Удалены записи PATH из $rc_file (резервная копия: $rc_file.bak)"
    fi
}

# Основной процесс удаления
remove_executable
remove_config
cleanup_rc

echo "Удаление завершено."
echo "Перезапустите терминал для применения изменений."
