#! /bin/bash
# while inotifywait -e close_write myfile.py; do ./myfile.py; done
src=o.log
inotifywait --monitor  \
            --timefmt '%d/%m/%y %H:%M' \
            --format '%T %w %f' \
            --excludei 'doc' \
            -e close_write ${src} |
    while read -r date time dir file; do
        clear
        changed_abs=${dir}${file}
        echo "At ${time} on ${date}, file $changed_abs was changed" >&2
        head o.log -n 20
        echo "Keep watching"
    done
