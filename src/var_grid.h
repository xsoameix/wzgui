#ifndef VAR_GRID_H
#define VAR_GRID_H

#include <gtk/gtk.h>
#include <portaudio.h>
#include <mpg123.h>
#include <wz/file.h>

typedef struct {
  wzvar * var;
  char * val;
} var_int_detail;

typedef struct {
  wzvar * var;
  char * val;
} var_flt_detail;

typedef struct {
  wzvar * var;
  char * bytes;
} var_str_detail;

typedef struct {
  wzvar * var;
  char * len;
} var_list_detail;

typedef struct {
  wzvar * var;
  char * len;
  char * w;
  char * h;
  char * data;
} var_img_detail;

typedef struct {
  char * x;
  char * y;
} var_2d;

typedef struct {
  wzvar * var;
  char * len;
  var_2d * vals;
} var_vex_detail;

typedef struct {
  wzvar * var;
  var_2d val;
} var_vec_detail;

typedef struct {
  wzvar * var;
  char * size;
  char * ms;
  uint16_t frame_size;
  uint32_t data_read;
  uint32_t data_size;
  uint8_t * data;
  PaStream * stream;
  GThread * thread;
  GRecMutex mutex;
  guint idle_id;
  int close;
  int draged_begin;
  int draged_end;
  int repeat;
  gdouble len;
  gdouble now;
  gdouble changed;
  GtkAdjustment * adjustment;
} var_ao_detail;

typedef struct {
  wzvar * var;
} var_uol_detail;

typedef union {
  wzvar ** var;
  var_int_detail * i;
  var_flt_detail * f;
  var_str_detail * str;
  var_list_detail * list;
  var_img_detail * img;
  var_vex_detail * vex;
  var_vec_detail * vec;
  var_ao_detail * ao;
  var_uol_detail * uol;
} var_detail;

G_BEGIN_DECLS

#define VAR_GRID_TYPE (var_grid_get_type())
#define VAR_GRID(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), VAR_GRID_TYPE, VarGrid))
#define VAR_GRID_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), VAR_GRID_TYPE, VarGrid))
#define IS_VAR_GRID(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), VAR_GRID_TYPE))
#define IS_VAR_GRID_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), VAR_GRID_TYPE))

typedef struct _VarGridPrivate VarGridPrivate;

typedef struct _VarGrid {
  GtkNotebook parent;
  VarGridPrivate * priv;
} VarGrid;

typedef struct _VarGridClass {
  GtkGridClass parent_class;
  void         (* set_type_detail)(VarGrid * grid, char * type);
  char *       (* get_type_detail)(VarGrid *);
  void         (* set_detail)(VarGrid * grid, var_detail detail);
  var_detail   (* get_detail)(VarGrid * grid);
} VarGridClass;

GType        var_grid_get_type(void) G_GNUC_CONST;
GtkWidget *  var_grid_new(void);
void         var_grid_set_type_detail(VarGrid *, char *);
char *       var_grid_get_type_detail(VarGrid *);
void         var_grid_set_detail(VarGrid *, var_detail);
var_detail   var_grid_get_detail(VarGrid *);

G_END_DECLS

#endif
