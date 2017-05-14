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

* [Aho-Corasick算法][4]
* 实例可选持久化，即在多次请求间使用同一个实例，避免重复加载关键字。
* 持久实例自动过期
* 客户程序主动控制实例生命周期
* 可选[Closure][3]作为回调函数，匹配到关键字时立即调用。回调函数可中断查找过程。

### Example

启动网络服务

```text
$ php -S 0.0.0.0:6060 -d "extension=modules/mss.so"
```

第一次访问

```text
$ curl http://127.0.0.1:6060/example/example.php

Load dict
mss_creation: 2012-10-18 12:37:52
...
```

第二次访问

```text
$ curl http://127.0.0.1:6060/example/example.php

mss_creation: 2012-10-18 12:37:52
...
```

第二次访问`Load dict`没有了，`mss_creation`不变。

当我们更新字典文件后再访问

```
$ touch example/example.dic
$ curl http://127.0.0.1:6060/example/example.php

Load dict
mss_creation: 2012-10-18 12:47:08
...
```

可以看到此时重新加载了字典，实例创建时间也是新的。

详见示例文件[example/example.php](example/example.php)

## License

Licensed under the [Apache License, Version 2.0](LICENSE)

*Copyright 2012 Anjuke Inc.*

### MultiFast

C library of Aho-Corasick Algorithm by [multifast][2] (LGPLv3).

  [1]: https://gist.github.com/1399772
  [2]: http://sourceforge.net/projects/multifast/
  [3]: http://multifast.sourceforge.net/library.php
  [4]: http://php.net/manual/en/functions.anonymous.php
  [5]: http://en.wikipedia.org/wiki/Aho%E2%80%93Corasick_string_matching_algorithm

