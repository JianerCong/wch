#! /bin/bash
# while inotifywait -e close_write myfile.py; do ./myfile.py; done

src=weak
bld=build-${src}
# rm ${bld} -rf
cmake -S ${src} -B ${bld}
# 🦜 reconfigure each time，this will take into account the change in cmake
# files.

# 🐢: Nope, it turns out if you want to change the configuration, you have to
# remove the build dir to force a clean re-build
inotifywait --monitor --recursive \
            --timefmt '%d/%m/%y %H:%M' \
            --format '%T %w %f' \
            --excludei 'doc' \
            -e close_write ${src} |
    while read -r date time dir file; do
        clear
        changed_abs=${dir}${file}
        echo "At ${time} on ${date}, file $changed_abs was changed" >&2
        cmake --build ${bld} 2> o.log
        echo "Keep watching"
    done
