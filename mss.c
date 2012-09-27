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

static int match_callback_closure(AC_MATCH_t *m, user_callback_param_t *ucp
        TSRMLS_DC) {
    static zval *invoke;
    zval *retval;

    if (!invoke) {
        ALLOC_INIT_ZVAL(invoke);
        ZVAL_STRING(invoke, "__invoke", 8);
    }

    zval **args[4];
    int argv;
    if (ucp->ext) {
        args[3] = &(ucp->ext);
        argv = 4;
    } else {
        argv = 3;
    }

    int i;
    for (i=0; i < m->match_num; i++) {
        AC_PATTERN_t *pattern = &(m->patterns[i]);

        zval *kw;
        zval *idx;
        zval *type;

        ALLOC_INIT_ZVAL(kw);
        ALLOC_INIT_ZVAL(idx);
        ALLOC_INIT_ZVAL(type);

        ZVAL_STRING(kw, pattern->astring, pattern->length);
        ZVAL_LONG(idx, m->position - pattern->length);
        if (pattern->rep.stringy) {
            ZVAL_STRING(type, pattern->rep.stringy, strlen(pattern->rep.stringy));
        } else {
            ZVAL_NULL(type);
        }

        args[0] = &kw;
        args[1] = &idx;
        args[2] = &type;

        if (call_user_function_ex(NULL, &(ucp->callback), invoke, &retval,
                argv, args, 0, NULL TSRMLS_CC) != SUCCESS) {
            zend_error(E_ERROR, "invoke callback failed");
        }

        zval_ptr_dtor(&type);
        zval_ptr_dtor(&idx);
        zval_ptr_dtor(&kw);

        if (Z_LVAL_P(retval)) {
            return 1;
        }
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
        zval *match;
        ALLOC_INIT_ZVAL(match);
        array_init(match);
        add_index_string(match, 0, pattern->astring, 1);
        add_index_long(match, 1, m->position - pattern->length);
        if (pattern->rep.stringy) {
            add_index_string(match, 2, pattern->rep.stringy, 1);
        }
        add_next_index_zval(matches, match);
    }

    return 0;
}

//

int le_mss, le_mss_persist;
#define PHP_MSS_RES_NAME "MSS resource"

typedef struct {
    AC_AUTOMATA_t *ac;
    time_t timestamp;
    zend_bool persist;
} mss_t;

static void mss_dtor(zend_rsrc_list_entry *rsrc TSRMLS_DC) {
    mss_t *mss = (mss_t *)rsrc->ptr;
    if (mss) {
        if (mss->ac) {
            ac_automata_release(mss->ac);
        }
        efree(mss);
    }
}

static void mss_persist_dtor(zend_rsrc_list_entry *rsrc TSRMLS_DC) {
    mss_t *mss = (mss_t *)rsrc->ptr;
    if (mss) {
        if (mss->ac) {
            ac_automata_release(mss->ac);
        }
        pefree(mss, 1);
    }
}

static zend_function_entry mss_functions[] = {
    PHP_FE(mss_create, NULL)
    PHP_FE(mss_is_ready, NULL)
    PHP_FE(mss_add, NULL)
    PHP_FE(mss_search, NULL)
    PHP_FE(mss_match, NULL)
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
    char *name = NULL;
    int name_len;
    long expiry = -1;

    zend_bool persist;

    mss_t *mss = NULL;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|sl",
            &name, &name_len, &expiry) == FAILURE) {
        RETURN_FALSE;
    }

    persist = name ? 1 : 0;

    if (persist) {
        zend_rsrc_list_entry *le;
        if (zend_hash_find(&EG(persistent_list), name, name_len + 1,
                (void **)&le) == SUCCESS) {
            mss = le->ptr;
            struct timeval tv;
            gettimeofday(&tv, NULL);
            if (expiry < 0 || tv.tv_sec - mss->timestamp <= expiry) {
                ZEND_REGISTER_RESOURCE(return_value, mss, le_mss_persist);
                return;
            };
        }
    }

    if (mss) {
        if (mss->ac) {
            ac_automata_release(mss->ac);
        }
        pefree(mss, persist);
    }

    mss = pemalloc(sizeof(mss_t), persist);

    mss->ac = ac_automata_init(match_callback);
    mss->persist = persist;

    struct timeval tv;
    gettimeofday(&tv, NULL);
    mss->timestamp = tv.tv_sec;

    if (persist) {
        ZEND_REGISTER_RESOURCE(return_value, mss, le_mss_persist);
        zend_rsrc_list_entry new_le;
        new_le.ptr = mss;
        new_le.type = le_mss_persist;
        zend_hash_add(&EG(persistent_list), name, name_len + 1, &new_le,
                sizeof(zend_rsrc_list_entry), NULL);
    } else {
        ZEND_REGISTER_RESOURCE(return_value, mss, le_mss);
    }
}

PHP_FUNCTION(mss_is_ready) {
    mss_t *mss;
    zval *zmss;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &zmss)
            == FAILURE) {
        RETURN_FALSE;
    }

    ZEND_FETCH_RESOURCE2(mss, mss_t*, &zmss, -1, PHP_MSS_RES_NAME, le_mss,
            le_mss_persist);

    RETURN_BOOL(!mss->ac->automata_open)
}

PHP_FUNCTION(mss_add) {
    mss_t *mss;
    zval *zmss;

    char *kw;
    int kw_len;

    char *type = NULL;
    int type_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rs|s", &zmss,
            &kw, &kw_len, &type, &type_len) == FAILURE) {
        RETURN_FALSE;
    }

    if (kw_len < 1) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING,
                "missing argument 'keyword'");
        RETURN_FALSE;
    }

    ZEND_FETCH_RESOURCE2(mss, mss_t*, &zmss, -1, PHP_MSS_RES_NAME, le_mss,
            le_mss_persist);

    AC_PATTERN_t pattern;
    pattern.astring = estrndup(kw, kw_len);
    pattern.length = kw_len;
    pattern.rep.stringy = type ? estrndup(type, strlen(type)) : NULL;

    ac_automata_add(mss->ac, &pattern);

    RETURN_TRUE;
}

PHP_FUNCTION(mss_search) {
    mss_t *mss;
    zval *zmss;
    AC_TEXT_t text;
    zval *callback = NULL;
    zval *ext = NULL;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rs|oz", &zmss,
            &text.astring, &text.length, &callback, &ext) == FAILURE) {
        RETURN_FALSE;
    }

    ZEND_FETCH_RESOURCE2(mss, mss_t*, &zmss, -1, PHP_MSS_RES_NAME, le_mss,
            le_mss_persist);

    if (mss->ac->automata_open) {
        ac_automata_finalize(mss->ac);
    }

    AC_AUTOMATA_t *rac = emalloc(sizeof(AC_AUTOMATA_t));
    memcpy(rac, mss->ac, sizeof(AC_AUTOMATA_t));

    ac_automata_reset(rac);

    match_callback_param_t mcp;

    if (callback) {
        user_callback_param_t ucp;
        ucp.callback = callback;
        ucp.ext = ext;
        mcp.type = MCP_TYPE_CLOSURE;
        mcp.value = &ucp;
        RETVAL_TRUE;
    } else {
        zval *matches = return_value;
        array_init(matches);
        mcp.type = MCP_TYPE_ARRAY;
        mcp.value = matches;
    }

    ac_automata_search(rac, &text, &mcp);

    efree(rac);
}

PHP_FUNCTION(mss_match) {
    mss_t *mss;
    zval *zmss;
    AC_TEXT_t text;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rs", &zmss,
            &text.astring, &text.length) == FAILURE) {
        RETURN_FALSE;
    }

    ZEND_FETCH_RESOURCE2(mss, mss_t*, &zmss, -1, PHP_MSS_RES_NAME, le_mss,
            le_mss_persist);

    if (mss->ac->automata_open) {
        ac_automata_finalize(mss->ac);
    }

    AC_AUTOMATA_t *rac = emalloc(sizeof(AC_AUTOMATA_t));
    memcpy(rac, mss->ac, sizeof(AC_AUTOMATA_t));

    ac_automata_reset(rac);

    match_callback_param_t mcp;
    zend_bool matched = 0;

    mcp.type = MCP_TYPE_MATCH;
    mcp.value = &matched;

    ac_automata_search(rac, &text, &mcp);

    efree(rac);

    RETURN_BOOL(matched);
}
