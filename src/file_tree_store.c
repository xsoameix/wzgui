#include "file_tree_store.h"

#define FILE_TREE_STORE_PRIVATE(obj) \
    (G_TYPE_INSTANCE_GET_PRIVATE((obj), FILE_TREE_STORE_TYPE, \
                                 FileTreeStorePrivate))

struct _FileTreeStorePrivate {
  PaStream * stream;
  char * filename;
  wzfile * file;
  wzctx * ctx;
};

G_DEFINE_TYPE(FileTreeStore, file_tree_store, GTK_TYPE_TREE_STORE)

FileTreeStore *
file_tree_store_new(void) {
  return FILE_TREE_STORE(g_object_new(file_tree_store_get_type(), NULL));
}

void
file_tree_store_set_filename(FileTreeStore * notebook, char * filename) {
  g_return_if_fail(notebook != NULL);
  g_return_if_fail(IS_FILE_TREE_STORE(notebook));
  FileTreeStorePrivate * priv = FILE_TREE_STORE_PRIVATE(notebook);
  priv->filename = filename;
}

char *
file_tree_store_get_filename(FileTreeStore * notebook) {
  g_return_if_fail(notebook != NULL);
  g_return_if_fail(IS_FILE_TREE_STORE(notebook));
  FileTreeStorePrivate * priv = FILE_TREE_STORE_PRIVATE(notebook);
  return priv->filename;
}

void
file_tree_store_set_wzfile(FileTreeStore * notebook, wzfile * file) {
  g_return_if_fail(notebook != NULL);
  g_return_if_fail(IS_FILE_TREE_STORE(notebook));
  FileTreeStorePrivate * priv = FILE_TREE_STORE_PRIVATE(notebook);
  priv->file = file;
}

wzfile *
file_tree_store_get_wzfile(FileTreeStore * notebook) {
  g_return_if_fail(notebook != NULL);
  g_return_if_fail(IS_FILE_TREE_STORE(notebook));
  FileTreeStorePrivate * priv = FILE_TREE_STORE_PRIVATE(notebook);
  return priv->file;
}

void
file_tree_store_set_wzctx(FileTreeStore * notebook, wzctx * ctx) {
  g_return_if_fail(notebook != NULL);
  g_return_if_fail(IS_FILE_TREE_STORE(notebook));
  FileTreeStorePrivate * priv = FILE_TREE_STORE_PRIVATE(notebook);
  priv->ctx = ctx;
}

wzctx *
file_tree_store_get_wzctx(FileTreeStore * notebook) {
  g_return_if_fail(notebook != NULL);
  g_return_if_fail(IS_FILE_TREE_STORE(notebook));
  FileTreeStorePrivate * priv = FILE_TREE_STORE_PRIVATE(notebook);
  return priv->ctx;
}

static void
file_tree_store_class_init(FileTreeStoreClass * klass) {
  klass->set_filename = file_tree_store_set_filename;
  klass->get_filename = file_tree_store_get_filename;
  klass->set_wzfile = file_tree_store_set_wzfile;
  klass->get_wzfile = file_tree_store_get_wzfile;
  klass->set_wzctx = file_tree_store_set_wzctx;
  klass->get_wzctx = file_tree_store_get_wzctx;
  g_type_class_add_private(klass, sizeof(FileTreeStorePrivate));
}

static void
file_tree_store_init(FileTreeStore * notebook) {
}
