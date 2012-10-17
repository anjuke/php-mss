#ifndef PHP_MSS_H
#define PHP_MSS_H 1

#define PHP_MSS_VERSION "0.1"
#define PHP_MSS_EXTNAME "mss"

PHP_MINIT_FUNCTION(mss);
PHP_FUNCTION(mss_create);
PHP_FUNCTION(mss_destroy);
PHP_FUNCTION(mss_timestamp);
PHP_FUNCTION(mss_is_ready);
PHP_FUNCTION(mss_add);
PHP_FUNCTION(mss_search);
PHP_FUNCTION(mss_match);

extern zend_module_entry mss_module_entry;
#define phpext_mss_ptr &mss_module_entry

#endif /* PHP_MSS_H */
