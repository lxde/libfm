#include <fm.h>

//ignore for test disabled asserts
#ifdef G_DISABLE_ASSERT
    #undef G_DISABLE_ASSERT
#endif

#define TEST_PARSING(str_to_parse, ...) \
    G_STMT_START { \
        char* expected[] = {__VA_ARGS__}; \
        test_parsing(str_to_parse, expected, G_N_ELEMENTS(expected)); \
    } G_STMT_END

static void test_parsing(const char* str, const char** expected, int n_expected)
{
    GSList* elements = NULL, *l;
    int i;
    FmPath *path, *element;
    path = fm_path_new(str);
    g_print("\ntry to parse \'%s\':\n[", str);

    for(element = path; element; element = element->parent)
        elements = g_slist_prepend(elements, element);
    for(i = 0, l = elements; l; l=l->next, ++i)
    {
        g_assert_cmpint(i, <, n_expected);
        element = (FmPath*)l->data;
        g_print("\'%s\'", element->name);
        if(l->next)
            g_print(", ");
        g_assert_cmpstr(element->name, ==, expected[i]);
    }
    g_slist_free(elements);
    g_print("]\n");

    g_assert_cmpint(i, ==, n_expected);

    fm_path_unref(path);
}

static void test_uri_parsing()
{
    FmPath* path;

    // test URIs
    TEST_PARSING("http://test/path/",
        "http://test/", "path");

    TEST_PARSING("http://test",
        "http://test/");

    TEST_PARSING("http://test/path/to/file",
        "http://test/", "path", "to", "file");

    // FIXME: is this ok?
    TEST_PARSING("http://test/path/to/file?test_arg=xx",
        "http://test/", "path", "to", "file?test_arg=xx");

    // test user name, password, and port
    TEST_PARSING("ftp://user@host",
        "ftp://user@host/");

    TEST_PARSING("ftp://user:pass@host",
        "ftp://user:pass@host/");

    TEST_PARSING("ftp://user@host:21",
        "ftp://user@host:21/");

    TEST_PARSING("ftp://user:pass@host:21",
        "ftp://user:pass@host:21/");

    TEST_PARSING("ftp://user:pass@host:21/path",
        "ftp://user:pass@host:21/", "path");

    TEST_PARSING("ftp://user:pass@host:21/path/",
        "ftp://user:pass@host:21/", "path");

    TEST_PARSING("ftp://user:pass@host:21/path//",
        "ftp://user:pass@host:21/", "path");

#if 0
    TEST_PARSING("ftp://user:pass@host:21/../",
        "ftp://user:pass@host:21/", "path");

    // test special locations
    TEST_PARSING("computer:",
        "computer:///");

    TEST_PARSING("computer:/",
        "computer:///");

    TEST_PARSING("computer:///",
        "computer:///");

    TEST_PARSING("computer://///",
        "computer:///");

    TEST_PARSING("computer://device",
        "computer:///", "device");

    TEST_PARSING("trash:",
        "trash:///");

    TEST_PARSING("trash:/",
        "trash:///");

    TEST_PARSING("trash://",
        "trash:///");

    TEST_PARSING("trash:///",
        "trash:///");

    TEST_PARSING("trash:///",
        "trash:///");

    TEST_PARSING("menu:");

    TEST_PARSING("menu:/");

    TEST_PARSING("menu://");

    TEST_PARSING("menu:///");

    TEST_PARSING("menu://application/");

    TEST_PARSING("menu://application/xxxx");

    TEST_PARSING("menu://application/xxxx/");
#endif

    // test invalid URIs, should fallback to root.
    TEST_PARSING("invalid_uri",
        "/");

    TEST_PARSING("invalid_uri:",
        "/");

    TEST_PARSING("invalid_uri:/",
        "/");

    TEST_PARSING("invalid_uri:/invalid",
        "/");

    TEST_PARSING("invalid_uri://///invalid",
        "/");

    TEST_PARSING("",
        "/");

    TEST_PARSING(NULL,
        "/");

}

static void test_path_parsing()
{
    FmPath* path;

    TEST_PARSING("/test/path",
        "/", "test", "path");

    TEST_PARSING("/test/path/",
        "/", "test", "path");

    TEST_PARSING("/test/path//",
        "/", "test", "path");

    TEST_PARSING("/test//path//",
        "/", "test", "path");

    TEST_PARSING("/test///path//",
        "/", "test", "path");

    TEST_PARSING("//test/path"
        "/", "test", "path");

    TEST_PARSING("//",
        "/");

    TEST_PARSING("////",
        "/");

    TEST_PARSING("/",
        "/");

    // canonicalize
    TEST_PARSING("/test/./path",
        "/", "test", "path");

    TEST_PARSING("/test/../path",
        "/", "path");

    TEST_PARSING("/../path",
        "/", "path");

    TEST_PARSING("/./path",
        "/", "path");

    TEST_PARSING("invalid_path",
        "/");

    TEST_PARSING("",
        "/");

    TEST_PARSING(NULL,
        "/");
}

int main (int   argc, char *argv[])
{
    g_type_init();
    fm_init(NULL);

    g_test_init (&argc, &argv, NULL); // initialize test program
    g_test_add_func("/FmPath/path_parsing", test_path_parsing);
    g_test_add_func("/FmPath/uri_parsing", test_uri_parsing);

    return g_test_run();
}

