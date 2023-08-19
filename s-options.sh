#! /bin/bash
# while inotifywait -e close_write myfile.py; do ./myfile.py; done
src=$1

watch=$2
if [ "$watch" = "" ]; then
    watch=$src
fi


echo "watching on $watch, source = $src"
bld=build-hi
cmake -S ${src} -B ${bld}
inotifywait --monitor --recursive \
            --timefmt '%d/%m/%y %H:%M' \
            --format '%T %w %f' \
            --excludei 'doc' \
            -e close_write ${watch} |
    while read -r date time dir file; do
        clear
        changed_abs=${dir}${file}
        echo "At ${time} on ${date}, file $changed_abs was changed" >&2
        # cmake --build ${bld} 2> o.txt
        cmake --build ${bld}
        echo "Keep watching"
    done
