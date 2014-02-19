if ../deps/uv/make.sh "$@"; then
    if test $1; then
        scp test/gadgetfs-test carambola:/tmp
    else
        scp gadgetfs carambola:/tmp
    fi
fi
