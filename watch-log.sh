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
        # 🦜: remove everything between "\x1b[33m" (S_YELLOW) and "\x1b[0m" (S_NOR) [inclusive]
        python3 ./process-log.py o.log > o-less.log
        head o-less.log -n 20   # 🦜 : Ignore warning
        echo "Keep watching"
    done
