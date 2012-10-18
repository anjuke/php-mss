<?php

/**
 * 创建一个新的mss实例，并返回实例的指针。如果持续化的同名实例已经存在且未过期会直接返回其指针
 * 而不再新建立一个。
 *
 * @param persist_name 如果提供了名字，mss将使用持久化模式。即在两个请求之间使用同一个
 *            mss实例。
 * @param expiry 这个mss实例的有效时间，以秒为单位。-1表示永不过期。
 *
 * @return mss实例指针
 */
function mss_create($persist_name=NULL, $expiry=NULL);

/**
 * 删除一个mss实例。非持久化的mss实例在请求结束时会自动销毁。持久化的mss实例如果需要销毁则必
 * 须显示调用这个方法。
 *
 * @param mss mss_create方法返回的mss实例指针
 */
function mss_destroy($mss);

/**
 * 返回该mss实例的创建时间戳。客户程序可以根据这个返回值自行判断
 *
 * @param mss mss实例指针
 *
 * @return 该mss实例的创建时间戳，unixtime形式的整型
 */
function mss_timestamp($mss);

/**
 * 返回这个mss实例是否已经准备好。准备好的实例不能够再调用mss_add()方法增加关键字。需要改变
 * 关键字必须重新建立一个实例。
 *
 * @param mss mss实例指针
 *
 * @return true or false
 */
function mss_is_ready($mss);

/**
 * 向mss增加匹配的关键字与改关键字的附属类型。附属类型在关键字被匹配时会返回用户程序，客户程序
 * 可以根据这个附属类型来决定对匹配到关键字的处理方式。
 *
 * @param mss mss实例指针
 * @param kw string 关键字
 * @param type string 附属类型
 */
function mss_add($mss, $kw, $type=NULL);

/**
 * 查找文本中包含的关键字，返回匹配到关键字在文本中的位置。支持以数组的形式返回所有查找结果，或
 * 者采用closure callback。
 *
 * 当callback参数为NULL时，将以数组形式返回。数组里的每个元素有2或3列。第一个为匹配到的关键字，
 * 第二个为该关键字在原文中的位置，第三个为该关键字在加入的设置的附属类型。
 *
 * 当callback参数不为空时，将在配到一个关键字调用一次客户程序提供的回调方法。回调函数的原型是
 * function($kw, $idx, $type==NULL, $extra==NULL)。回调函数返回一个整型值，当其返回值
 * 为0表示继续下一个匹配查找；否则搜索过程中断。
 *
 * @param mss mss实例指针
 * @param text string 待查找的文本
 * @param callback closure 回调函数
 * @param extra zval 附属变量，回调函数可以利用这个变量确定是哪个查找在调用其
 *
 * @return array or bool
 */
function mss_search($mss, $text, $callback=NULL, $extra=NULL);

/**
 * 判断文本中是否包含关键字。
 *
 * @param mss  mss实例指针
 * @param text 待查找的文本
 *
 * @return bool true 文本中包含任意一个关键字。false 文本中没有包含关键字。
 */
function mss_match($mss, $text);
