#include <fm.h>

//ignore for test disabled asserts
#ifdef G_DISABLE_ASSERT
    #undef G_DISABLE_ASSERT
#endif

static void double_slash_test_case(void)
{
    FmPath* path = fm_path_new("//tmp/lest");
    g_assert(path);
    fm_path_unref(path);
}

int
main (int   argc, char *argv[])
{
    g_type_init();
    g_test_init (&argc, &argv, NULL); // initialize test program
    g_test_add_func ("/BasePath/Base Path New with Double Slash at beggining",
    double_slash_test_case);
    fm_init(NULL);
    return g_test_run();
}

