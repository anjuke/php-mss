<?php
// header("content-type: text/plain; charset=utf-8");

$mss = mss_create();
mss_add($mss, "安居客", "公司名称");
mss_add($mss, "二手房", "通用名词");

$text = "安居客，是安居客集团旗下国内最大的专业二手房网站。自2007年上线至今的短短4年时间里，安居客凭借其“专业二手房搜索引擎”的核心竞争力在业内独树一帜。通过对用户需求的精准把握、海量的二手房房源、精准的搜索功能、强大的产品研发能力，为用户提供最佳找房体验。目前，安居客的足迹已经遍布全国超过29个城市，注册用户超过1000万。";

echo "Round: 1\n";
$matches = mss_search($mss, $text);
echo "[\n";
foreach ($matches as $match) {
    echo "  ({$match[0]}, {$match[2]}, {$match[1]})\n";
}
echo "]\n";

echo "\n";
echo "Round: 2\n";
echo "[\n";
mss_search($mss, $text, function($kw, $idx, $type) {
    echo "  ($kw, $idx, $type)\n";
});
echo "]\n";

echo "\n";
$matched = mss_match($mss, $text);
echo $matched ? "matched" : "not matched", "\n";