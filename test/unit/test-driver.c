#include <string.h>
#include <gcutter.h>
#include <glib/gstdio.h>
#include "driver.h"

#define TEST_ENTER do { GRN_LOG(ctx, GRN_LOG_DEBUG,             \
                                "<%s>", __FUNCTION__);} while(0)

static gchar *base_directory;
static gchar *tmp_directory;
static grn_ctx *ctx;

void cut_startup()
{
  base_directory = g_get_current_dir();
  tmp_directory = g_build_filename(base_directory, "/tmp", NULL);
  cut_remove_path(tmp_directory, NULL);
  g_mkdir_with_parents(tmp_directory, 0700);
  g_chdir(tmp_directory);
  ctx = (grn_ctx*) g_malloc(sizeof(grn_ctx));
}

void cut_shutdown()
{
  g_chdir(base_directory);
  g_free(tmp_directory);
  g_free(ctx);
}

void cut_setup()
{
  mrn_init();
  grn_ctx_init(ctx,0);
  grn_ctx_use(ctx, mrn_db);
}

void cut_teardown()
{
  grn_ctx_fin(ctx);
  mrn_deinit();
}

void test_mrn_flush_logs()
{
  TEST_ENTER;
  cut_assert_equal_int(0, mrn_flush_logs(ctx));
}

void test_mrn_hash_put()
{
  TEST_ENTER;
  const char *key1 = "aa";
  const char *key2 = "bbb";
  const char *key3 = "cccc";
  const char *key4 = "primitive";
  char *value1 = "11ddbb";
  char *value2 = "22fefe2";
  char *value3 = "3333r";
  int value4 = 777;

  cut_assert_equal_int(0,mrn_hash_put(ctx, key1, (void*) value1));
  cut_assert_equal_int(0,mrn_hash_put(ctx, key2, (void*) value2));
  cut_assert_equal_int(0,mrn_hash_put(ctx, key3, (void*) value3));
  cut_assert_equal_int(0,mrn_hash_put(ctx, key4, (void*) &value4));
  cut_assert_equal_int(-1,mrn_hash_put(ctx, key1, (void*) value2));
  cut_assert_equal_int(-1,mrn_hash_put(ctx, key2, (void*) value3));
  cut_assert_equal_int(-1,mrn_hash_put(ctx, key3, (void*) value1));
}

void test_mrn_hash_get()
{
  TEST_ENTER;
  const char *key1 = "aa";
  const char *key2 = "bbb";
  const char *key3 = "cccc";
  const char *key4 = "not found";
  const char *key5 = "primitive";
  char *value1 = "abcdefg";
  char *value2 = "hijklmnopq";
  char *value3 = "rxyz012";
  int value5 = 777;
  char *res;
  int *res_int;

  mrn_hash_put(ctx, key1, (void*) value1);
  mrn_hash_put(ctx, key2, (void*) value2);
  mrn_hash_put(ctx, key3, (void*) value3);
  mrn_hash_put(ctx, key5, (void*) &value5);

  cut_assert_equal_int(0, mrn_hash_get(ctx, key1, (void**) &res));
  cut_assert_equal_string(value1, res);
  cut_assert_equal_int(0, mrn_hash_get(ctx, key2, (void**) &res));
  cut_assert_equal_string(value2, res);
  cut_assert_equal_int(0, mrn_hash_get(ctx, key3, (void**) &res));
  cut_assert_equal_string(value3, res);

  cut_assert_equal_int(-1, mrn_hash_get(ctx, key4, (void**) &res));

  cut_assert_equal_int(0, mrn_hash_get(ctx, key5, (void**) &res_int));
  cut_assert_equal_int(value5, *res_int);
}

void test_mrn_hash_remove()
{
  TEST_ENTER;
  const char *key1 = "aa";
  const char *key2 = "bbb";
  const char *key3 = "cccc";
  const char *key4 = "not found";
  const char *key5 = "primitive";
  char *value1 = "112233";
  char *value2 = "2221115";
  char *value3 = "333344";
  int value5 = 777;

  mrn_hash_put(ctx, key1, (void*) value1);
  mrn_hash_put(ctx, key2, (void*) value2);
  mrn_hash_put(ctx, key3, (void*) value3);
  mrn_hash_put(ctx, key5, (void*) &value5);

  cut_assert_equal_int(-1, mrn_hash_remove(ctx, key4));
  cut_assert_equal_int(0, mrn_hash_remove(ctx, key1));
  cut_assert_equal_int(0, mrn_hash_remove(ctx, key2));
  cut_assert_equal_int(0, mrn_hash_remove(ctx, key3));
  cut_assert_equal_int(0, mrn_hash_remove(ctx, key5));

  cut_assert_equal_int(-1, mrn_hash_remove(ctx, key1));
  cut_assert_equal_int(-1, mrn_hash_remove(ctx, key2));
  cut_assert_equal_int(-1, mrn_hash_remove(ctx, key3));
  cut_assert_equal_int(-1, mrn_hash_remove(ctx, key5));
}

void test_mrn_init_obj_info()
{
  TEST_ENTER;
  uint n_columns = 8, i;
  mrn_info *info;
  info = mrn_init_obj_info(ctx, n_columns);
  cut_assert_not_null(info);
  info->table->name = "hoge";
  info->table->name_size = 4;
  cut_assert_true(info->table->flags == GRN_OBJ_PERSISTENT);
  cut_assert_true(info->table->value_size == GRN_TABLE_MAX_KEY_SIZE);
  cut_assert_null(info->table->obj);
  cut_assert_equal_int(n_columns, info->n_columns);
  for (i=0; i < n_columns; i++)
  {
    info->columns[i]->name = "fuga";
    info->columns[i]->name_size = 4;
    cut_assert_true(info->columns[i]->flags == GRN_OBJ_PERSISTENT);
    cut_assert_null(info->columns[i]->obj);
  }
  mrn_deinit_obj_info(ctx, info);
}

void test_mrn_deinit_obj_info()
{
  TEST_ENTER;
  mrn_info *info = mrn_init_obj_info(ctx, 8);
  cut_assert_not_null(info);
  cut_assert_equal_int(0, mrn_deinit_obj_info(ctx, info));
}

void test_mrn_create()
{
  TEST_ENTER;
  grn_obj *obj, *obj2;
  mrn_info *info = mrn_init_obj_info(ctx, 2);

  info->table->name = "test/t1";
  info->table->name_size = strlen("test/t1");
  info->table->flags |= GRN_OBJ_TABLE_NO_KEY;

  info->columns[0]->name = "c1";
  info->columns[0]->name_size = strlen("c1");
  info->columns[0]->flags |= GRN_OBJ_COLUMN_SCALAR;
  info->columns[0]->type = grn_ctx_at(ctx, GRN_DB_INT32);

  info->columns[1]->name = "c2";
  info->columns[1]->name_size = strlen("c2");
  info->columns[1]->flags |= GRN_OBJ_COLUMN_SCALAR;
  info->columns[1]->type = grn_ctx_at(ctx, GRN_DB_TEXT);

  cut_assert_equal_int(0, mrn_create(ctx, info));

  cut_assert_not_null((obj = grn_ctx_get(ctx, "test/t1", strlen("test/t1"))));
  cut_assert_not_null((obj2 = grn_obj_column(ctx, obj, "c1", strlen("c1"))));
  grn_obj_remove(ctx, obj2);
  cut_assert_not_null((obj2 = grn_obj_column(ctx, obj, "c2", strlen("c2"))));
  grn_obj_remove(ctx, obj2);
  cut_assert_null((obj2 = grn_obj_column(ctx, obj, "c3", strlen("c3"))));
  grn_obj_remove(ctx, obj);

  mrn_deinit_obj_info(ctx, info);
}

void test_mrn_open()
{
  TEST_ENTER;
  grn_obj *obj,*obj2;

  mrn_info *info = mrn_init_obj_info(ctx, 2);

  info->table->name = "test/t1";
  info->table->name_size = strlen("test/t1");
  info->table->flags |= GRN_OBJ_TABLE_NO_KEY;

  info->columns[0]->name = "c1";
  info->columns[0]->name_size = strlen("c1");
  info->columns[0]->flags |= GRN_OBJ_COLUMN_SCALAR;
  info->columns[0]->type = grn_ctx_at(ctx, GRN_DB_INT32);

  info->columns[1]->name = "c2";
  info->columns[1]->name_size = strlen("c2");
  info->columns[1]->flags |= GRN_OBJ_COLUMN_SCALAR;
  info->columns[1]->type = grn_ctx_at(ctx, GRN_DB_TEXT);

  cut_assert_equal_int(0, mrn_create(ctx, info));
  cut_assert_null(info->table->obj);
  cut_assert_null(info->columns[0]->obj);
  cut_assert_null(info->columns[1]->obj);
  cut_assert_equal_int(0, mrn_open(ctx, info));
  cut_assert_not_null(info->table->obj);
  cut_assert_not_null(info->columns[0]->obj);
  cut_assert_not_null(info->columns[1]->obj);

  grn_obj_remove(ctx, info->columns[1]->obj);
  grn_obj_remove(ctx, info->columns[0]->obj);
  grn_obj_remove(ctx, info->table->obj);
  mrn_deinit_obj_info(ctx, info);
}

void test_mrn_close()
{
  TEST_ENTER;
  grn_obj *obj,*obj2;

  mrn_info *info = mrn_init_obj_info(ctx, 2);

  info->table->name = "test/t1";
  info->table->name_size = strlen("test/t1");
  info->table->flags |= GRN_OBJ_TABLE_NO_KEY;

  info->columns[0]->name = "c1";
  info->columns[0]->name_size = strlen("c1");
  info->columns[0]->flags |= GRN_OBJ_COLUMN_SCALAR;
  info->columns[0]->type = grn_ctx_at(ctx, GRN_DB_INT32);

  info->columns[1]->name = "c2";
  info->columns[1]->name_size = strlen("c2");
  info->columns[1]->flags |= GRN_OBJ_COLUMN_SCALAR;
  info->columns[1]->type = grn_ctx_at(ctx, GRN_DB_TEXT);

  cut_assert_equal_int(0, mrn_create(ctx, info));
  cut_assert_equal_int(0, mrn_open(ctx, info));
  cut_assert_not_null(info->table->obj);
  cut_assert_not_null(info->columns[0]->obj);
  cut_assert_not_null(info->columns[1]->obj);

  cut_assert_equal_int(0, mrn_close(ctx, info));
  cut_assert_null(info->table->obj);
  cut_assert_null(info->columns[0]->obj);
  cut_assert_null(info->columns[1]->obj);

  cut_assert_equal_int(0, mrn_open(ctx, info));
  grn_obj_remove(ctx, info->columns[1]->obj);
  grn_obj_remove(ctx, info->columns[0]->obj);
  grn_obj_remove(ctx, info->table->obj);
  mrn_deinit_obj_info(ctx, info);
}

void test_mrn_drop()
{
   TEST_ENTER;
  grn_obj *obj, *obj2;
  mrn_info *info = mrn_init_obj_info(ctx, 2);

  info->table->name = "test/t1";
  info->table->name_size = strlen("test/t1");
  info->table->flags |= GRN_OBJ_TABLE_NO_KEY;

  info->columns[0]->name = "c1";
  info->columns[0]->name_size = strlen("c1");
  info->columns[0]->flags |= GRN_OBJ_COLUMN_SCALAR;
  info->columns[0]->type = grn_ctx_at(ctx, GRN_DB_INT32);

  info->columns[1]->name = "c2";
  info->columns[1]->name_size = strlen("c2");
  info->columns[1]->flags |= GRN_OBJ_COLUMN_SCALAR;
  info->columns[1]->type = grn_ctx_at(ctx, GRN_DB_TEXT);

  cut_assert_equal_int(0, mrn_create(ctx, info));

  cut_assert_equal_int(0, mrn_drop(ctx, info));

  cut_assert_null(info->table->obj);
  cut_assert_null(info->columns[0]->obj);
  cut_assert_null(info->columns[1]->obj);

  mrn_deinit_obj_info(ctx, info);
}
