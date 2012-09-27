# README

一个实现[多关键字的文本精确匹配搜][1]的PHP扩展。


## Quick Start

### Build
```text
$ git clone git://github.com/erning/php-mss.git
$ cd php-mss
$ phpize
$ ./configure
$ make
```

### Run
```text
$ cat sample.php | php -d "extension=modules/mss.so"

Round: 1
[
  (安居客, 公司名称, 0)
  (安居客, 公司名称, 15)
  (二手房, 通用名词, 57)
  (安居客, 公司名称, 122)
  (二手房, 通用名词, 149)
  (二手房, 通用名词, 263)
  (安居客, 公司名称, 380)
]
...

```

## Features

## License

### MultiFast

C library of Aho-Corasick Algorithm by [multifast][2] (LGPLv3).

  [1]: https://gist.github.com/1399772
  [2]: http://sourceforge.net/projects/multifast/

