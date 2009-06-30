#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "driver.h"
#include "config.h"

grn_hash *mrn_hash;
grn_obj *mrn_db, *mrn_lexicon;
pthread_mutex_t *mrn_lock, *mrn_lock_hash;
const char *mrn_logfile_name=MRN_LOG_FILE_NAME;
FILE *mrn_logfile = NULL;

uint mrn_hash_counter=0;

grn_logger_info mrn_logger_info = {
  GRN_LOG_DUMP,
  GRN_LOG_TIME|GRN_LOG_MESSAGE,
  mrn_logger_func,
  NULL
};

void mrn_logger_func(int level, const char *time, const char *title,
		     const char *msg, const char *location, void *func_arg)
{
  const char slev[] = " EACewnid-";
  if ((mrn_logfile)) {
    fprintf(mrn_logfile, "%s|%c|%u|%s\n", time,
	    *(slev + level), (uint)pthread_self(), msg);
    fflush(mrn_logfile);
  }
}

int mrn_flush_logs(grn_ctx *ctx)
{
  pthread_mutex_lock(mrn_lock);
  GRN_LOG(ctx, GRN_LOG_NOTICE, "flush logfile");
  if (fflush(mrn_logfile) || fclose(mrn_logfile)
      || (mrn_logfile = fopen(mrn_logfile_name, "a")) == NULL)
  {
    GRN_LOG(ctx, GRN_LOG_ERROR, "cannot flush logfile");
    return -1;
  }
  pthread_mutex_unlock(mrn_lock);
  return 0;
}

int mrn_init()
{
  grn_ctx ctx;

  // init groonga
  if (grn_init() != GRN_SUCCESS)
  {
    goto err;
  }

  grn_ctx_init(&ctx,0);

  // init log, and then we can do logging
  if (!(mrn_logfile = fopen(mrn_logfile_name, "a")))
  {
    goto err;
  }

  grn_logger_info_set(&ctx, &mrn_logger_info);
  GRN_LOG(&ctx, GRN_LOG_NOTICE, "%s start", PACKAGE_STRING);

  // init database
  struct stat dummy;
  if ((stat(MRN_DB_FILE_PATH, &dummy)))
  {
    GRN_LOG(&ctx, GRN_LOG_NOTICE, "database not exists");
    if ((mrn_db = grn_db_create(&ctx, MRN_DB_FILE_PATH, NULL)))
    {
      GRN_LOG(&ctx, GRN_LOG_NOTICE, "database created");
    }
    else
    {
      GRN_LOG(&ctx, GRN_LOG_ERROR, "cannot create database, exiting");
      goto err;
    }
  }
  else
  {
    mrn_db = grn_db_open(&ctx, MRN_DB_FILE_PATH);
  }
  grn_ctx_use(&ctx, mrn_db);


  // init lexicon table
  if (!(mrn_lexicon = grn_ctx_get(&ctx,"lexicon",7)))
  {
    GRN_LOG(&ctx, GRN_LOG_NOTICE, "lexicon table not exists");
    if ((mrn_lexicon = grn_table_create(&ctx, MRN_LEXICON_TABLE_NAME,
                                        strlen(MRN_LEXICON_TABLE_NAME), NULL,
                                        GRN_OBJ_TABLE_PAT_KEY|GRN_OBJ_PERSISTENT,
                                        grn_ctx_at(&ctx,GRN_DB_SHORTTEXT), 0)))
    {
      GRN_LOG(&ctx, GRN_LOG_NOTICE, "lexicon table created");
    }
    else
    {
      GRN_LOG(&ctx, GRN_LOG_ERROR, "cannot create lexicon table, exiting");
      goto err;
    }
  }

  // init hash
  if (!(mrn_hash = grn_hash_create(&ctx,NULL,
                                   MRN_MAX_KEY_LEN,sizeof(size_t),
                                   GRN_OBJ_KEY_VAR_SIZE)))
  {
    GRN_LOG(&ctx, GRN_LOG_ERROR, "cannot init hash, exiting");
    goto err;
  }

  // init lock
  mrn_lock = malloc(sizeof(pthread_mutex_t));
  if ((mrn_lock == NULL) || (pthread_mutex_init(mrn_lock, NULL) != 0))
  {
    goto err;
  }
  mrn_lock_hash = malloc(sizeof(pthread_mutex_t));
  if ((mrn_lock_hash == NULL) || (pthread_mutex_init(mrn_lock_hash, NULL) != 0))
  {
    goto err;
  }

  grn_ctx_fin(&ctx);
  return 0;

err:
  // TODO: report more detail error to mysql
  grn_ctx_fin(&ctx);
  return -1;
}

int mrn_deinit()
{
  grn_ctx ctx;
  grn_ctx_init(&ctx,0);
  grn_ctx_use(&ctx, mrn_db);
  GRN_LOG(&ctx, GRN_LOG_NOTICE, "shutdown");


  pthread_mutex_destroy(mrn_lock);
  free(mrn_lock);
  mrn_lock = NULL;
  pthread_mutex_destroy(mrn_lock_hash);
  free(mrn_lock_hash);
  mrn_lock_hash = NULL;

  grn_hash_close(&ctx, mrn_hash);
  mrn_hash = NULL;

  grn_obj_close(&ctx, mrn_lexicon);
  mrn_lexicon = NULL;

  grn_db_close(&ctx, mrn_db);
  mrn_db = NULL;

  fclose(mrn_logfile);
  mrn_logfile = NULL;

  grn_ctx_fin(&ctx);
  grn_fin();

  return 0;
}

/**
 *   0 success
 *  -1 duplicated
 */
int mrn_hash_put(grn_ctx *ctx, const char *key, void *value)
{
  int added, res=0;
  void *buf;
  pthread_mutex_lock(mrn_lock_hash);
  grn_hash_add(ctx, mrn_hash, (const char*) key, strlen(key), &buf, &added);
  // duplicate check
  if (added == 0)
  {
    GRN_LOG(ctx, GRN_LOG_WARNING, "hash put duplicated (key=%s)", key);
    res = -1;
  }
  else
  {
    // store address of value
    memcpy(buf, &value, sizeof(buf));
    mrn_hash_counter++;
  }
  pthread_mutex_unlock(mrn_lock_hash);
  return res;
}

/**
 *   0 success
 *  -1 not found
 */
int mrn_hash_get(grn_ctx *ctx, const char *key, void **value)
{
  int res = 0;
  grn_id id;
  grn_search_flags flags = 0;
  void *buf;
  pthread_mutex_lock(mrn_lock_hash);
  id = grn_hash_lookup(ctx, mrn_hash, (const char*) key, strlen(key), &buf, &flags);
  // key not found
  if (id == GRN_ID_NIL)
  {
    GRN_LOG(ctx, GRN_LOG_WARNING, "hash get not found (key=%s)", key);
    res = -1;
  }
  else
  {
    // restore address of value
    memcpy(value, buf, sizeof(buf));
  }
  pthread_mutex_unlock(mrn_lock_hash);
  return res;
}

/**
 *   0 success
 *  -1 error
 */
int mrn_hash_remove(grn_ctx *ctx, const char *key)
{
  int res = 0;
  grn_rc rc;
  grn_id id;
  grn_search_flags flags = 0;
  pthread_mutex_lock(mrn_lock_hash);
  id = grn_hash_lookup(ctx, mrn_hash, (const char*) key, strlen(key), NULL, &flags);
  if (id == GRN_ID_NIL)
  {
    GRN_LOG(ctx, GRN_LOG_WARNING, "hash remove not found (key=%s)", key);
    res = -1;
  }
  else
  {
    rc = grn_hash_delete_by_id(ctx, mrn_hash, id, NULL);
    if (rc != GRN_SUCCESS) {
      GRN_LOG(ctx, GRN_LOG_ERROR, "hash remove error (key=%s)", key);
      res = -1;
    }
    else
    {
      mrn_hash_counter--;
    }
  }
  pthread_mutex_unlock(mrn_lock_hash);
  return res;
}

mrn_info *mrn_init_obj_info(grn_ctx *ctx, uint n_columns)
{
  int i, alloc_size = 0;
  void *ptr;
  mrn_info *info;
  alloc_size = sizeof(mrn_info) + sizeof(mrn_table_info) +
    (sizeof(void*) + sizeof(mrn_column_info)) * n_columns;
  ptr = malloc(alloc_size);
  if (ptr == NULL)
  {
    GRN_LOG(ctx, GRN_LOG_ERROR, "malloc error mrn_init_create_info size=%d",alloc_size);
    return NULL;
  }
  info = (mrn_info*) ptr;
  ptr += sizeof(mrn_info);

  info->table = ptr;
  info->table->name = NULL;
  info->table->name_size = 0;
  info->table->path = NULL;
  info->table->flags = GRN_OBJ_PERSISTENT;
  info->table->key_type = NULL;
  info->table->value_size = GRN_TABLE_MAX_KEY_SIZE;
  info->table->obj = NULL;

  ptr += sizeof(mrn_table_info);
  info->columns = (mrn_column_info**) ptr;

  ptr += sizeof(void*) * n_columns;
  for (i = 0; i < n_columns; i++)
  {
    info->columns[i] = ptr + sizeof(mrn_column_info) * i;
    info->columns[i]->name = NULL;
    info->columns[i]->name_size = 0;
    info->columns[i]->path = NULL;
    info->columns[i]->flags = GRN_OBJ_PERSISTENT;
    info->columns[i]->type = NULL;
    info->columns[i]->obj = NULL;
  }

  info->n_columns = n_columns;
  info->ref_count = 0;
  return info;
}

int mrn_deinit_obj_info(grn_ctx *ctx, mrn_info *info)
{
  free(info);
  return 0;
}

int mrn_create(grn_ctx *ctx, mrn_info *info)
{
  int i;
  mrn_table_info *table;
  mrn_column_info *column;

  table = info->table;

  table->obj = grn_table_create(ctx, table->name, table->name_size,
                                table->path, table->flags,
                                table->key_type, table->value_size);
  if (table->obj == NULL)
  {
    GRN_LOG(ctx, GRN_LOG_ERROR, "cannot create table: name=%s, name_size=%d, path=%s, "
            "flags=%d, key_type=%p, value_size=%d", table->name, table->name_size,
            table->path, table->flags, table->key_type, table->value_size);
    return -1;
  }
  for (i=0; i < info->n_columns; i++)
  {
    column = info->columns[i];
    column->obj = grn_column_create(ctx, table->obj, column->name,
                                    column->name_size, column->path,
                                    column->flags, column->type);
    if (column->obj == NULL)
    {
      GRN_LOG(ctx, GRN_LOG_ERROR, "cannot create column: table=%p, name=%s, name_size=%d, "
              "path=%s, flags=%d, type=%p", table->obj, column->name,
              column->name_size, column->path,
              column->flags, column->type);
      goto auto_drop;
    }
    else
    {
      grn_obj_close(ctx, column->obj);
      column->obj = NULL;
    }
  }
  grn_obj_close(ctx, table->obj);
  table->obj = NULL;
  return 0;

auto_drop:
  GRN_LOG(ctx, GRN_LOG_ERROR, "auto-drop table/columns");
  for (; i > 0; --i)
  {
    grn_obj_remove(ctx, info->columns[i]->obj);
    info->columns[i]->obj = NULL;
  }
  grn_obj_remove(ctx, info->table->obj);
  info->table->obj = NULL;
  return -1;
}

int mrn_open(grn_ctx *ctx, mrn_info *info)
{
  int i;
  mrn_table_info *table;
  mrn_column_info *column;

  table = info->table;

  table->obj = grn_ctx_get(ctx, table->name, table->name_size);
  if (table->obj == NULL)
  {
    GRN_LOG(ctx, GRN_LOG_ERROR, "cannot open table: name=%s", table->name);
    return -1;
  }
  for (i=0; i < info->n_columns; i++)
  {
    column = info->columns[i];
    column->obj = grn_obj_column(ctx, table->obj, column->name, column->name_size);
    if (column->obj == NULL)
    {
      GRN_LOG(ctx, GRN_LOG_ERROR, "cannot open column: table=%s, column=%s",
              table->name, column->name);
      goto auto_close;
    }
  }
  return 0;

auto_close:
  GRN_LOG(ctx, GRN_LOG_ERROR, "auto-closing table/columns");
  for (; i > 0; --i)
  {
    grn_obj_close(ctx, info->columns[i]->obj);
    info->columns[i]->obj = NULL;
  }
  grn_obj_close(ctx, info->table->obj);
  info->table->obj = NULL;
  return -1;
}

int mrn_close(grn_ctx *ctx, mrn_info *info)
{
  int i;
  for (i=0; i < info->n_columns; i++)
  {
    grn_obj_close(ctx, info->columns[i]->obj);
    info->columns[i]->obj = NULL;
  }
  grn_obj_close(ctx, info->table->obj);
  info->table->obj = NULL;
  return 0;
}

int mrn_drop(grn_ctx *ctx, mrn_info *info)
{
  if (mrn_open(ctx, info) == 0)
  {
    int i;
    for (i=0; i < info->n_columns; i++)
    {
      grn_obj_remove(ctx, info->columns[i]->obj);
      info->columns[i]->obj = NULL;
    }
    grn_obj_remove(ctx, info->table->obj);
    info->table->obj = NULL;
    return 0;
  }
  GRN_LOG(ctx, GRN_LOG_ERROR, "cannot open table=%s duaring drop process", info->table->name);
  return -1;
}
