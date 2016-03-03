#ifndef FILE_TREE_STORE_H
#define FILE_TREE_STORE_H

#include <gtk/gtk.h>
#include <portaudio.h>
#include <wz/file.h>

G_BEGIN_DECLS

#define FILE_TREE_STORE_TYPE (file_tree_store_get_type())
#define FILE_TREE_STORE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), FILE_TREE_STORE_TYPE, FileTreeStore))
#define FILE_TREE_STORE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), FILE_TREE_STORE_TYPE, FileTreeStore))
#define IS_FILE_TREE_STORE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), FILE_TREE_STORE_TYPE))
#define IS_FILE_TREE_STORE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), FILE_TREE_STORE_TYPE))

typedef struct _FileTreeStorePrivate FileTreeStorePrivate;

typedef struct _FileTreeStore {
  GtkNotebook parent;
  FileTreeStorePrivate * priv;
} FileTreeStore;

typedef struct _FileTreeStoreClass {
  GtkNotebookClass parent_class;
  void        (* set_filename)(FileTreeStore * notebook, char * filename);
  char *      (* get_filename)(FileTreeStore * notebook);
  void        (* set_wzfile)(FileTreeStore * notebook, wzfile * file);
  wzfile *    (* get_wzfile)(FileTreeStore * notebook);
  void        (* set_wzctx)(FileTreeStore * notebook, wzctx * ctx);
  wzctx *     (* get_wzctx)(FileTreeStore * notebook);
} FileTreeStoreClass;

GType           file_tree_store_get_type(void) G_GNUC_CONST;
FileTreeStore * file_tree_store_new(void);
void            file_tree_store_set_filename(FileTreeStore *, char *);
char *          file_tree_store_get_filename(FileTreeStore *);
void            file_tree_store_set_wzfile(FileTreeStore *, wzfile *);
wzfile *        file_tree_store_get_wzfile(FileTreeStore *);
void            file_tree_store_set_wzctx(FileTreeStore *, wzctx *);
wzctx *         file_tree_store_get_wzctx(FileTreeStore *);

G_END_DECLS

#endif
