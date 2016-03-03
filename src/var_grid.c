#include "var_grid.h"

#define VAR_GRID_PRIVATE(obj) \
    (G_TYPE_INSTANCE_GET_PRIVATE((obj), VAR_GRID_TYPE, \
                                 VarGridPrivate))

struct _VarGridPrivate {
  char * type;
  var_detail detail;
};

G_DEFINE_TYPE(VarGrid, var_grid, GTK_TYPE_GRID)

GtkWidget *
var_grid_new(void) {
  return GTK_WIDGET(g_object_new(var_grid_get_type(), NULL));
}

void
var_grid_set_type_detail(VarGrid * grid, char * type) {
  g_return_if_fail(grid != NULL);
  g_return_if_fail(IS_VAR_GRID(grid));
  VarGridPrivate * priv = VAR_GRID_PRIVATE(grid);
  priv->type = type;
}

char *
var_grid_get_type_detail(VarGrid * grid) {
  g_return_if_fail(grid != NULL);
  g_return_if_fail(IS_VAR_GRID(grid));
  VarGridPrivate * priv = VAR_GRID_PRIVATE(grid);
  return priv->type;
}

void
var_grid_set_detail(VarGrid * grid, var_detail detail) {
  g_return_if_fail(grid != NULL);
  g_return_if_fail(IS_VAR_GRID(grid));
  VarGridPrivate * priv = VAR_GRID_PRIVATE(grid);
  priv->detail = detail;
}

var_detail
var_grid_get_detail(VarGrid * grid) {
  g_return_if_fail(grid != NULL);
  g_return_if_fail(IS_VAR_GRID(grid));
  VarGridPrivate * priv = VAR_GRID_PRIVATE(grid);
  return priv->detail;
}

static void
var_grid_class_init(VarGridClass * klass) {
  klass->set_type_detail = var_grid_set_type_detail;
  klass->get_type_detail = var_grid_get_type_detail;
  klass->set_detail = var_grid_set_detail;
  klass->get_detail = var_grid_get_detail;
  g_type_class_add_private(klass, sizeof(VarGridPrivate));
}

static void
var_grid_init(VarGrid * grid) {
}
