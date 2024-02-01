# 🦜 : same as s-hi.sh but clean build at the begining
#! /bin/bash
# while inotifywait -e close_write myfile.py; do ./myfile.py; done
src=weak
bld=build-${src}
rm ${bld} -rf
cmake -S ${src} -B ${bld}
inotifywait --monitor --recursive \
            --timefmt '%d/%m/%y %H:%M' \
            --format '%T %w %f' \
            --excludei '(doc|flycheck|#)' \
            -e close_write ${src} |
    while read -r date time dir file; do
        clear
        changed_abs=${dir}${file}
        echo "At ${time} on ${date}, file $changed_abs was changed" >&2
        cmake --build ${bld} 2> o.log
        echo "Keep watching"
    done
