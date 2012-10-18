/*
   Copyright 2012 Anjuke Inc.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#ifndef PHP_MSS_H
#define PHP_MSS_H 1

#define PHP_MSS_VERSION "1.0"
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
