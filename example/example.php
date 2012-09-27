<?php

function load_dict($mss, $filename) {
    $fp = fopen($filename, "r");
    while (!feof($fp)) {
        $line = trim(fgets($fp));
        $pos = strpos($line, "#");
        if ($pos !== false) {
            $line = strstr($line, 0, $pos);
        }
        if (!$line) {
            continue;
        }
        $line = explode(":", $line, 2);
        if (count($line) == 2) {
            mss_add($mss, trim($line[0]), trim($line[1]));
        } else {
            mss_add($mss, trim($line[0]));
        }
    }
}


$mss = mss_create("example", 60);

if (!mss_is_ready($mss)) {
    load_dict($mss, "example.dic");
}

$text = file_get_contents("example.txt");

//
//
echo "mms_match(): \n";
$ret = mss_match($mss, $text);
echo "    ", ($ret ? "matched" : "not matched"), "\n";
echo "\n";

//
//
echo "mms_search() array:\n";
$ret = mss_search($mss, $text);
echo "    ", "count: ", count($ret), "\n";
echo "\n";

//
//
echo "mms_search() callback: \n";
$count = 0;
$ret = mss_search($mss, $text, function($kw, $idx, $type, $ext) {
    $ext[0]++;
    for ($i = strlen($kw) - 1; $i >= 0; $i--) {
        $ext[1][$idx + $i] = '*';
    }
}, array(&$count, &$text));
echo "    ", "count: ", $count, "\n";
echo "\n";

//
//
echo "mms_match(): \n";
$ret = mss_match($mss, $text);
echo "    ", ($ret ? "matched" : "not matched"), "\n";
echo "\n";
