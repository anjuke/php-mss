PHP_ARG_ENABLE(mss, [Whether to enable mss support],
    [ --enable-mss Enable mss support])

if test "$PHP_MSS" = "yes"; then
    PHP_NEW_EXTENSION(mss, mss.c ahocorasick.c node.c, $ext_shared)
fi