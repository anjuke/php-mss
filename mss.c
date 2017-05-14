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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/time.h>

#include <php.h>
#include <php_ini.h>

#include "php_mss.h"
#include "ahocorasick.h"

//

#define MCP_TYPE_MATCH   1
#define MCP_TYPE_CLOSURE 2
#define MCP_TYPE_ARRAY   3

typedef struct {
    int type;
    void *value;
} match_callback_param_t;

typedef struct {
    zval *callback;
    zval *ext;
} user_callback_param_t;

static int match_callback_closure(AC_MATCH_t *m, user_callback_param_t *ucp TSRMLS_DC) {
    zval invoke;
#if PHP_MAJOR_VERSION < 7
    zval *retval;
#else
    zval retval;
#endif
//Closure::__invoke()
#if PHP_MAJOR_VERSION < 7
    ZVAL_STRING(&invoke, "__invoke", 8);
#else
    ZVAL_STRING(&invoke, "__invoke");
    //ZVAL_STRINGL(&invoke, "__invoke", 8);
#endif

#if PHP_MAJOR_VERSION < 7
    zval **args[4];
#else
    zval args[4];
#endif
    int argv;
    if (ucp->ext) {
#if PHP_MAJOR_VERSION < 7
        args[3] = &(ucp->ext);
#else
        args[3] = *(ucp->ext);  //$ext => function(.....,$p4){}
#endif
        argv = 4;
    } else {
        argv = 3;  //function($p1,$p2,$p3){}
    }

    int i;
    for (i=0; i < m->match_num; i++) {
        AC_PATTERN_t *pattern = &(m->patterns[i]);

        zval *kw;
        zval *idx;
        zval *type;

#if PHP_MAJOR_VERSION < 7
        ALLOC_INIT_ZVAL(kw);
        ALLOC_INIT_ZVAL(idx);
        ALLOC_INIT_ZVAL(type);
#else
        kw = ecalloc(sizeof(zval), 1);
        idx = ecalloc(sizeof(zval), 1);
        type = ecalloc(sizeof(zval), 1);
#endif

#if PHP_MAJOR_VERSION < 7
        ZVAL_STRING(kw, pattern->astring, pattern->length);
#else
        ZVAL_STRING(kw, pattern->astring);
#endif
        ZVAL_LONG(idx, m->position - pattern->length);
        if (pattern->rep.stringy) {
#if PHP_MAJOR_VERSION < 7
            ZVAL_STRING(type, pattern->rep.stringy, strlen(pattern->rep.stringy));
#else
            ZVAL_STRING(type, pattern->rep.stringy);
#endif
        } else {
            ZVAL_NULL(type);
        }

#if PHP_MAJOR_VERSION < 7
        args[0] = &kw;
        args[1] = &idx;
        args[2] = &type;
#else
        args[0] = *kw;
        args[1] = *idx;
        args[2] = *type;
#endif

        int call_result;
#if PHP_MAJOR_VERSION < 7
        if (call_user_function_ex(NULL, &(ucp->callback), &invoke, &retval,
                argv, args, 0, NULL TSRMLS_CC) != SUCCESS) {
#else
        if ((call_result = call_user_function_ex(NULL, ucp->callback, &invoke, &retval,
                argv, args, 0, NULL TSRMLS_CC)) != SUCCESS) {
#endif
            zend_error(E_ERROR, "invoke callback failed");
        }

#if PHP_MAJOR_VERSION < 7
        zval_ptr_dtor(&type);
        zval_ptr_dtor(&idx);
        zval_ptr_dtor(&kw);
#else
        zval_ptr_dtor(type);
        zval_ptr_dtor(idx);
        zval_ptr_dtor(kw);
#endif

#if PHP_MAJOR_VERSION < 7
        if (Z_LVAL_P(retval)) {
#else
        if (call_result == SUCCESS && Z_TYPE(retval) != IS_UNDEF && zval_is_true(&retval)) {
#endif
            return 1;
        }
#if PHP_MAJOR_VERSION < 7
#else
        zval_ptr_dtor(&retval);
#endif
    }

    return 0;
}

static int match_callback(AC_MATCH_t *m, void *param TSRMLS_DC) {
    match_callback_param_t *mcp = (match_callback_param_t *)param;

    if (mcp->type == MCP_TYPE_MATCH) {
        *(zend_bool *)(mcp->value) = 1;
        return 1;
    }

    if (mcp->type == MCP_TYPE_CLOSURE) {

        return match_callback_closure(m,
                (user_callback_param_t *)(mcp->value) TSRMLS_CC);

    }

    // MCP_TYPE_ARRAY
    zval *matches = (zval *)(mcp->value);
    int i;
    for (i=0; i < m->match_num; i++) {
        AC_PATTERN_t *pattern = &(m->patterns[i]);
#if PHP_MAJOR_VERSION < 7
        zval *match;
        ALLOC_INIT_ZVAL(match);
        array_init(match);
#else
        zval match;
        array_init(&match);
#endif

#if PHP_MAJOR_VERSION < 7
        add_index_string(match, 0, pattern->astring, 1);
        add_index_long(match, 1, m->position - pattern->length);
#else
        add_index_string(&match, 0, pattern->astring);
        add_index_long(&match, 1, m->position - pattern->length);
#endif
        if (pattern->rep.stringy) {
#if PHP_MAJOR_VERSION < 7
            add_index_string(match, 2, pattern->rep.stringy, 1);
#else
            add_index_string(&match, 2, pattern->rep.stringy);
#endif
        }

#if PHP_MAJOR_VERSION < 7
        add_next_index_zval(matches, match);
#else
        add_next_index_zval(matches, &match);
#endif
    }

    return 0;
}

//
int le_mss, le_mss_persist;
#define PHP_MSS_RES_NAME "MSS resource"

typedef struct {
    void *value;
    void *next;
} list_item_t;

typedef struct {
    AC_AUTOMATA_t *ac;
    union {
        char *name;
        int persist;
    };
    time_t timestamp;
    list_item_t *head;
    list_item_t *tail;
} mss_t;

static void mss_free(mss_t *mss TSRMLS_DC) {
    if (!mss) {
        return;
    }

    if (mss->ac) {
        ac_automata_release(mss->ac);
    }

    if (mss->name) {
        pefree(mss->name, mss->persist);
    }

    list_item_t *curr = mss->head;
    while (curr) {
        list_item_t *next = curr->next;
        if (curr->value) {
            pefree(curr->value, mss->persist);
        }
        pefree(curr, mss->persist);
        curr = next;
    }

    pefree(mss, mss->persist);
}

#if PHP_MAJOR_VERSION < 7
static void mss_dtor(zend_rsrc_list_entry *rsrc TSRMLS_DC) {
#else
static void mss_dtor(zend_resource *rsrc TSRMLS_DC) {
#endif
    mss_t *mss = (mss_t *)rsrc->ptr;
    mss_free(mss TSRMLS_CC);
}

#if PHP_MAJOR_VERSION < 7
static void mss_persist_dtor(zend_rsrc_list_entry *rsrc TSRMLS_DC) {
#else
static void mss_persist_dtor(zend_resource *rsrc TSRMLS_DC) {
#endif
    mss_t *mss = (mss_t *)rsrc->ptr;
    mss_free(mss TSRMLS_CC);
}

static zend_function_entry mss_functions[] = {
    PHP_FE(mss_create, NULL)
    PHP_FE(mss_destroy, NULL)
    PHP_FE(mss_timestamp, NULL)
    PHP_FE(mss_is_ready, NULL)
    PHP_FE(mss_add, NULL)
    PHP_FE(mss_search, NULL)
    PHP_FE(mss_match, NULL)
    PHP_FE(mss_display, NULL)
    {NULL, NULL, NULL}
};

zend_module_entry mss_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
    STANDARD_MODULE_HEADER,
#endif
    PHP_MSS_EXTNAME,
    mss_functions,      /* Functions */
    PHP_MINIT(mss),     /* MINIT     */
    NULL,               /* MSHUTDOWN */
    NULL,               /* RINIT     */
    NULL,               /* RSHUTDOWN */
    NULL,               /* MINFO     */
#if ZEND_MODULE_API_NO >= 20010901
    PHP_MSS_VERSION,
#endif
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_MSS
ZEND_GET_MODULE(mss)
#endif

PHP_MINIT_FUNCTION(mss) {
    le_mss = zend_register_list_destructors_ex(mss_dtor, NULL,
            PHP_MSS_RES_NAME, module_number);
    le_mss_persist = zend_register_list_destructors_ex(NULL,
            mss_persist_dtor, PHP_MSS_RES_NAME, module_number);

    return SUCCESS;
}

PHP_FUNCTION(mss_create) {
#if PHP_MAJOR_VERSION < 7
    char *name = NULL;
    int name_len = 0;
#else
    char* name = NULL;
    size_t name_len = 0;
#endif
    long expiry = -1;

    zend_bool persist;

    mss_t *mss = NULL;

#if PHP_MAJOR_VERSION < 7
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|sl",
            &name, &name_len, &expiry) == FAILURE) {
#else
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|sl",
            &name, &name_len, &expiry) == FAILURE) {
#endif
        RETURN_FALSE;
    }

    persist = name ? 1 : 0;

    if (persist) {
#if PHP_MAJOR_VERSION < 7
        zend_rsrc_list_entry *le;
#else
        zend_resource *le;
#endif
#if PHP_MAJOR_VERSION < 7
        if (zend_hash_find(&EG(persistent_list), name, name_len + 1,
                (void **)&le) == SUCCESS) {
#else
        if ((le = zend_hash_str_find_ptr(&EG(persistent_list), name, name_len + 1))) {
#endif
            mss = le->ptr;
            struct timeval tv;
            gettimeofday(&tv, NULL);
            if (expiry < 0 || tv.tv_sec - mss->timestamp < expiry) {
#if PHP_MAJOR_VERSION < 7
                ZEND_REGISTER_RESOURCE(return_value, mss, le_mss_persist);
                return;
#else
                RETURN_RES(zend_register_resource(mss, le_mss_persist));
#endif
            }
#if PHP_MAJOR_VERSION < 7
            zend_hash_del(&EG(persistent_list), name, name_len + 1);
#else
            //zend_hash_del(&EG(persistent_list), name);
            zend_hash_str_del(&EG(persistent_list), name, name_len + 1);
#endif
        }
    }

    mss = pemalloc(sizeof(mss_t), persist);

    //mss->ac = ac_automata_init(match_callback);//1.0.0
    mss->ac = ac_automata_init();//1.3.1

    struct timeval tv;
    gettimeofday(&tv, NULL);
    mss->timestamp = tv.tv_sec;

    mss->head = pemalloc(sizeof(list_item_t), persist);
    mss->head->value = NULL;
    mss->head->next = NULL;
    mss->tail = mss->head;

    if (persist) {
        mss->name = pestrndup(name, name_len + 1, persist);
#if PHP_MAJOR_VERSION < 7
        ZEND_REGISTER_RESOURCE(return_value, mss, le_mss_persist);
#else
        zend_resource* new_le = zend_register_resource(mss, le_mss_persist);
        RETVAL_RES(new_le);
#endif
#if PHP_MAJOR_VERSION < 7
        zend_rsrc_list_entry new_le;
        new_le.ptr = mss;
        new_le.type = le_mss_persist;
#else
        new_le->ptr = mss;
        new_le->type = le_mss_persist;
#endif
#if PHP_MAJOR_VERSION < 7
        zend_hash_add(&EG(persistent_list), name, name_len + 1, &new_le,
                sizeof(zend_rsrc_list_entry), NULL);
#else
        zend_hash_str_update_mem(&EG(persistent_list), name, name_len + 1, new_le, sizeof(zend_resource));
#endif
    } else {
        mss->name = NULL;
#if PHP_MAJOR_VERSION < 7
        ZEND_REGISTER_RESOURCE(return_value, mss, le_mss);
#else
        RETURN_RES(zend_register_resource(mss, le_mss));
#endif
    }

}

PHP_FUNCTION(mss_destroy) {
    mss_t *mss;
    zval *zmss;

#if PHP_MAJOR_VERSION < 7
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &zmss)
            == FAILURE) {
#else
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &zmss)
            == FAILURE) {
#endif
        RETURN_FALSE;
    }

#if PHP_MAJOR_VERSION < 7
    ZEND_FETCH_RESOURCE2(mss, mss_t*, &zmss, -1, PHP_MSS_RES_NAME, le_mss,
            le_mss_persist);
#else
    if((mss = (mss_t *)zend_fetch_resource2(Z_RES_P(zmss), PHP_MSS_RES_NAME, le_mss, le_mss_persist)) == NULL)
    {
        RETURN_FALSE;
    }
#endif

    if (mss && mss->persist) {
#if PHP_MAJOR_VERSION < 7
        zend_hash_del(&EG(persistent_list), mss->name, strlen(mss->name) + 1);
#else
        zend_hash_str_del(&EG(persistent_list), mss->name, strlen(mss->name) + 1);
#endif
        RETURN_TRUE;
    }
    RETURN_FALSE;
}

PHP_FUNCTION(mss_timestamp) {
    mss_t *mss;
    zval *zmss;

#if PHP_MAJOR_VERSION < 7
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &zmss)
            == FAILURE) {
#else
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &zmss)
            == FAILURE) {
#endif
        RETURN_FALSE;
    }

#if PHP_MAJOR_VERSION < 7
    ZEND_FETCH_RESOURCE2(mss, mss_t*, &zmss, -1, PHP_MSS_RES_NAME, le_mss,
            le_mss_persist);
#else
    if((mss = (mss_t *)zend_fetch_resource2(Z_RES_P(zmss), PHP_MSS_RES_NAME, le_mss, le_mss_persist)) == NULL)
    {
        RETURN_FALSE;
    }
#endif

    RETURN_LONG(mss->timestamp);
}

PHP_FUNCTION(mss_is_ready) {
    mss_t *mss;
    zval *zmss;

#if PHP_MAJOR_VERSION < 7
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &zmss)
            == FAILURE) {
#else
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &zmss)
            == FAILURE) {
#endif
        RETURN_FALSE;
    }

#if PHP_MAJOR_VERSION < 7
    ZEND_FETCH_RESOURCE2(mss, mss_t*, &zmss, -1, PHP_MSS_RES_NAME, le_mss,
            le_mss_persist);
#else
    if((mss = (mss_t *)zend_fetch_resource2(Z_RES_P(zmss), PHP_MSS_RES_NAME, le_mss, le_mss_persist)) == NULL)
    {
        RETURN_FALSE;
    }
#endif

    RETURN_BOOL(!mss->ac->automata_open)
}

PHP_FUNCTION(mss_add) {
    mss_t *mss;
    zval *zmss;

#if PHP_MAJOR_VERSION < 7
    char *kw;
    int kw_len;

    char *type = NULL;
    int type_len = 0;
#else
    char *kw;
    size_t kw_len;

    char *type = NULL;
    size_t type_len = 0;
#endif

#if PHP_MAJOR_VERSION < 7
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rs|s", &zmss,
            &kw, &kw_len, &type, &type_len) == FAILURE) {
#else
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rs|s", &zmss,
            &kw, &kw_len, &type, &type_len) == FAILURE) {
#endif
        RETURN_FALSE;
    }

    if (kw_len < 1) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING,
                "missing argument 'keyword'");
        RETURN_FALSE;
    }

#if PHP_MAJOR_VERSION < 7
    ZEND_FETCH_RESOURCE2(mss, mss_t*, &zmss, -1, PHP_MSS_RES_NAME, le_mss,
            le_mss_persist);
#else
    if((mss = (mss_t *)zend_fetch_resource2(Z_RES_P(zmss), PHP_MSS_RES_NAME, le_mss, le_mss_persist)) == NULL)
    {
        RETURN_FALSE;
    }
#endif

    AC_PATTERN_t pattern;

    pattern.astring = pestrndup(kw, kw_len, mss->persist);
    pattern.length = kw_len;
    pattern.rep.stringy = type
            ? pestrndup(type, type_len, mss->persist)
            : NULL;

    list_item_t *item = pemalloc(sizeof(list_item_t), mss->persist);
    item->value = pattern.astring;
    item->next = NULL;
    mss->tail->next = item;
    mss->tail = item;

    if (pattern.rep.stringy) {
        item = pemalloc(sizeof(list_item_t), mss->persist);
        item->value = pattern.rep.stringy;
        item->next = NULL;
        mss->tail->next = item;
        mss->tail = item;
    }

    ac_automata_add(mss->ac, &pattern);

    RETURN_TRUE;
}

PHP_FUNCTION(mss_search) {
    mss_t *mss;
    zval *zmss;
    AC_TEXT_t text;
    zval *callback = NULL;
    zval *ext = NULL;

#if PHP_MAJOR_VERSION < 7
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rs|oz", &zmss,
            &text.astring, &text.length, &callback, &ext) == FAILURE) {
        RETURN_FALSE;
    }
#else
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rs|oz", &zmss,
            &text.astring, &text.length, &callback, &ext) == FAILURE) {
        RETURN_FALSE;
    }
#endif

#if PHP_MAJOR_VERSION < 7
    ZEND_FETCH_RESOURCE2(mss, mss_t*, &zmss, -1, PHP_MSS_RES_NAME, le_mss,
            le_mss_persist);
#else
    if((mss = (mss_t *)zend_fetch_resource2(Z_RES_P(zmss), PHP_MSS_RES_NAME, le_mss, le_mss_persist)) == NULL)
    {
        RETURN_FALSE;
    }
#endif

    if (mss->ac->automata_open) {
        ac_automata_finalize(mss->ac);
    }

    AC_AUTOMATA_t ac;
    memcpy(&ac, mss->ac, sizeof(AC_AUTOMATA_t));
    //ac_automata_reset(&ac);//1.0.0

    match_callback_param_t mcp;

    zend_bool matched = 0;

    if (callback) {
        user_callback_param_t ucp;
        ucp.callback = callback;
        ucp.ext = ext;
        mcp.type = MCP_TYPE_CLOSURE;
        mcp.value = &ucp;

#if PHP_MAJOR_VERSION < 7
        RETVAL_TRUE;
#else
        //ac_automata_search(&ac, &text, &mcp);//1.0.0
        ac_automata_search(&ac, &text, 0, match_callback, &mcp);//1.3.1
        RETURN_TRUE;//应立即返回
#endif

    } else {
        zval *matches = return_value;
        array_init(matches);
        mcp.type = MCP_TYPE_ARRAY;
        mcp.value = matches;
    }

    ac_automata_search(&ac, &text, 0, match_callback, &mcp);

    //efree(&ac);
}

PHP_FUNCTION(mss_match) {
    mss_t *mss;
    zval *zmss;
    AC_TEXT_t text;

#if PHP_MAJOR_VERSION < 7
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rs", &zmss,
            &text.astring, &text.length) == FAILURE) {
#else
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rs", &zmss,
            &text.astring, &text.length) == FAILURE) {
#endif
        RETURN_FALSE;
    }

#if PHP_MAJOR_VERSION < 7
    ZEND_FETCH_RESOURCE2(mss, mss_t*, &zmss, -1, PHP_MSS_RES_NAME, le_mss,
            le_mss_persist);
#else
    if((mss = (mss_t *)zend_fetch_resource2(Z_RES_P(zmss), PHP_MSS_RES_NAME, le_mss, le_mss_persist)) == NULL)
    {
        RETURN_FALSE;
    }
#endif

    if (mss->ac->automata_open) {
        ac_automata_finalize(mss->ac);
    }

    AC_AUTOMATA_t *rac = emalloc(sizeof(AC_AUTOMATA_t));
    memcpy(rac, mss->ac, sizeof(AC_AUTOMATA_t));

    //ac_automata_reset(rac);//1.0.0

    match_callback_param_t mcp;
    zend_bool matched = 0;

    mcp.type = MCP_TYPE_MATCH;
    mcp.value = &matched;

    ac_automata_search(rac, &text, 0, match_callback, &mcp);

    efree(rac);

    RETURN_BOOL(matched);
}

PHP_FUNCTION(mss_display) {
    mss_t *mss;
    zval *zmss;

#if PHP_MAJOR_VERSION < 7
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &zmss)
            == FAILURE) {
#else
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &zmss)
            == FAILURE) {
#endif
        RETURN_FALSE;
    }

#if PHP_MAJOR_VERSION < 7
    ZEND_FETCH_RESOURCE2(mss, mss_t*, &zmss, -1, PHP_MSS_RES_NAME, le_mss,
            le_mss_persist);
#else
    if((mss = (mss_t *)zend_fetch_resource2(Z_RES_P(zmss), PHP_MSS_RES_NAME, le_mss, le_mss_persist)) == NULL)
    {
        RETURN_FALSE;
    }
#endif

    if (mss->ac->automata_open) {
        ac_automata_finalize(mss->ac);
    }

    AC_AUTOMATA_t *rac = emalloc(sizeof(AC_AUTOMATA_t));
    memcpy(rac, mss->ac, sizeof(AC_AUTOMATA_t));

    //ac_automata_reset(rac);//1.0.0

    ac_automata_display(rac, 's');

}
